#include "big_text.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_map>

#include <QColor>

// ============================== 常量 =======================================
// 扫光：周期 3.2 秒，其中穿越耗时 2.4 秒
static constexpr float kSweep = 2.4f;
static constexpr float kPeriod = 3.2f;

static const char* const kPaletteHex[BigText::kPaletteN] = {
    "#28282e",  // 阴影线（暗底上隐约可见）
    "#707088",  // 字形的暗色
    "#c8c8dc",  // 字形的亮色
    "#ffffff",  // 扫光峰值
};

// ============================== 字形表 =====================================
// 每个字形 5 行，'#' 是墨。宽度不一：I 是 1 格宽，M 是 5 格宽。
using Glyph5 = std::array<std::string, 5>;

static const std::unordered_map<char, Glyph5>& FontMap() {
  static const std::unordered_map<char, Glyph5> kFont = {
      {'A', {".##.", "#..#", "####", "#..#", "#..#"}},
      {'B', {"###.", "#..#", "###.", "#..#", "###."}},
      {'C', {".###", "#...", "#...", "#...", ".###"}},
      {'D', {"###.", "#..#", "#..#", "#..#", "###."}},
      {'E', {"####", "#...", "###.", "#...", "####"}},
      {'F', {"####", "#...", "###.", "#...", "#..."}},
      {'G', {".###", "#...", "#.##", "#..#", ".###"}},
      {'H', {"#..#", "#..#", "####", "#..#", "#..#"}},
      {'I', {"#", "#", "#", "#", "#"}},
      {'J', {"...#", "...#", "...#", "#..#", ".##."}},
      {'K', {"#..#", "#.#.", "##..", "#.#.", "#..#"}},
      {'L', {"#...", "#...", "#...", "#...", "####"}},
      {'M', {"#...#", "##.##", "#.#.#", "#...#", "#...#"}},
      {'N', {"#...#", "##..#", "#.#.#", "#..##", "#...#"}},
      {'O', {".##.", "#..#", "#..#", "#..#", ".##."}},
      {'P', {"###.", "#..#", "###.", "#...", "#..."}},
      {'Q', {".##.", "#..#", "#..#", "#.#.", ".#.#"}},
      {'R', {"###.", "#..#", "###.", "#.#.", "#..#"}},
      {'S', {".###", "#...", ".##.", "...#", "###."}},
      {'T', {"#####", "..#..", "..#..", "..#..", "..#.."}},
      {'U', {"#..#", "#..#", "#..#", "#..#", ".##."}},
      {'V', {"#...#", "#...#", "#...#", ".#.#.", "..#.."}},
      {'W', {"#...#", "#...#", "#.#.#", "##.##", "#...#"}},
      {'X', {"#...#", ".#.#.", "..#..", ".#.#.", "#...#"}},
      {'Y', {"#...#", ".#.#.", "..#..", "..#..", "..#.."}},
      {'Z', {"####", "...#", "..#.", ".#..", "####"}},
      {'0', {"###", "#.#", "#.#", "#.#", "###"}},
      {'1', {".#.", "##.", ".#.", ".#.", "###"}},
      {'2', {"###", "..#", "###", "#..", "###"}},
      {'3', {"###", "..#", ".##", "..#", "###"}},
      {'4', {"#.#", "#.#", "###", "..#", "..#"}},
      {'5', {"###", "#..", "###", "..#", "###"}},
      {'6', {"###", "#..", "###", "#.#", "###"}},
      {'7', {"###", "..#", "..#", "..#", "..#"}},
      {'8', {"###", "#.#", "###", "#.#", "###"}},
      {'9', {"###", "#.#", "###", "..#", "###"}},
      {'.', {".", ".", ".", ".", "#"}},
      {',', {".", ".", ".", "#", "#"}},
      {'!', {"#", "#", "#", ".", "#"}},
      {'?', {"###", "..#", ".##", "...", ".#."}},
      {'\'', {"#", "#", ".", ".", "."}},
      {':', {".", "#", ".", "#", "."}},
      {'-', {"...", "...", "###", "...", "..."}},
      {'+', {"...", ".#.", "###", ".#.", "..."}},
      {'=', {"...", "###", "...", "###", "..."}},
      {'/', {"..#", "..#", ".#.", "#..", "#.."}},
      {'_', {"...", "...", "...", "...", "###"}},
      {' ', {".."}},
  };
  return kFont;
}

// 网格上取字形墨：越界返回 0
static inline int InkAt(const std::vector<uint8_t>& kind, int c, int r) {
  if (c < 0 || c >= BigText::kWidth || r < 0 || r >= BigText::kHeight)
    return 0;
  return kind[r * BigText::kWidth + c] == 2 ? 1 : 0;
}

// ============================== 公开接口 ===================================
const char* const* BigText::PaletteHex() { return kPaletteHex; }

BigText::BigText(const std::string& text) { Init(text); }

// ============================== 静态构建 ===================================
void BigText::Init(const std::string& text) {
  for (int i = 0; i < kPaletteN; ++i) {
    QColor c(kPaletteHex[i]);
    palette_[i][0] = c.redF();
    palette_[i][1] = c.greenF();
    palette_[i][2] = c.blueF();
  }

  const int N = kWidth * kHeight;
  kind_.assign(N, 0);
  step_.assign(N, 0);
  color_.assign(N, 0);

  // 收集字形
  std::vector<Glyph5> glyphs;
  for (char c : text) {
    char u = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    auto it = FontMap().find(u);
    if (it != FontMap().end())
      glyphs.push_back(it->second);
  }
  if (glyphs.empty()) {
    x_left_ = 0;
    x_right_ = kWidth - 1;
    return;
  }

  // 每个字形像素占 2 列 × 1 行，让方块看上去是方的；
  // 如果放不下就退化成 1 列宽
  auto span = [&](int s) {
    int total = -1;
    for (const auto& g : glyphs) {
      total += static_cast<int>(g[0].size()) * s + 2;
    }
    return total;
  };
  const int s = (span(2) <= kWidth - 2) ? 2 : 1;
  const int totalSpan = span(s);
  const int x0 = std::max(0, (kWidth - totalSpan) / 2);
  const int y0 = (kHeight - 6) / 2;  // (8-6)/2 = 1

  // 把字形渲染成墨
  int x = x0;
  for (const auto& g : glyphs) {
    for (int y = 0; y < 5; ++y) {
      const std::string& line = g[y];
      for (int i = 0; i < static_cast<int>(line.size()); ++i) {
        if (line[i] != '#')
          continue;
        for (int kk = 0; kk < s; ++kk) {
          int xx = x + i * s + kk;
          if (xx < kWidth)
            kind_[(y0 + y) * kWidth + xx] = 2;
        }
      }
    }
    x += static_cast<int>(g[0].size()) * s + 2;
  }

  // 阴影：字母轮廓向右下挪半格，于是每个空格画它左上角相接的边。
  // 这里只区分"是不是阴影"，方向由字符退化成一个小点。
  for (int r = 0; r < kHeight; ++r) {
    for (int c = 0; c < kWidth; ++c) {
      const int k = r * kWidth + c;
      if (kind_[k] == 2)
        continue;
      const int a = InkAt(kind_, c - 1, r - 1);
      const int b = InkAt(kind_, c, r - 1);
      const int d = InkAt(kind_, c - 1, r);
      const int edges =
          (a != b ? 1 : 0) | (d ? 2 : 0) | (a != d ? 4 : 0) | (b ? 8 : 0);
      if (edges != 0)
        kind_[k] = 1;
    }
  }

  // 扫光左右端点
  x_left_ = x0 - 2;
  x_right_ = x0 + totalSpan + 9;
}

// ---------- 每帧 -----------------------------------------------------------
void BigText::Update(double t) {
  const float tf = static_cast<float>(t);

  // 扫光匀速穿过，然后歇在视野外。t=0 是静止的。
  float u = std::fmod(tf - 0.5f, kPeriod);
  if (u < 0.f)
    u += kPeriod;
  u /= kSweep;
  const float p = (u < 1.f) ? (static_cast<float>(x_left_) +
                               static_cast<float>(x_right_ - x_left_) * u)
                            : -99.f;

  const int N = kWidth * kHeight;
  for (int r = 0; r < kHeight; ++r) {
    for (int c = 0; c < kWidth; ++c) {
      const int k = r * kWidth + c;
      const uint8_t kind = kind_[k];
      if (kind == 2) {
        // 字形：离扫光越近越亮、点越大
        const float d = std::abs(c + 1.2f * r - p);
        if (d < 1.2f) {
          step_[k] = 3;
          color_[k] = 3;  // 扫光峰值
        } else if (d < 2.4f) {
          step_[k] = 3;
          color_[k] = 2;  // 亮字
        } else {
          step_[k] = 2;
          color_[k] = 1;  // 暗字
        }
      } else if (kind == 1) {
        step_[k] = 1;
        color_[k] = 0;  // 阴影小点
      } else {
        step_[k] = 0;
        color_[k] = 0;
      }
    }
  }
}