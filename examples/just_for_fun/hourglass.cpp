#include "hourglass.h"

#include <algorithm>
#include <cmath>

// ============================== 常量 =======================================
static constexpr float kW = 0.6f, kH = 1.2f;  // 格子尺寸（em）
static constexpr int kL = 9;                  // 球部长度
static constexpr float kRN = 0.36f, kRM = 5.0f, kRC = 4.3f;
static constexpr int kPX = 6;       // 立柱从中线向外
static constexpr float kK = 0.6f;   // 沙的坡度
static constexpr float kLT = 6.2f;  // 全部在上时沙面高度
static constexpr float kRUN = 10.5f, kHOLD = 1.2f;
static constexpr float kTurn[3] = {0.2f, 0.45f, 0.2f};
static constexpr float kCycle = kRUN + kHOLD + kTurn[0] + kTurn[1] + kTurn[2];
static constexpr float kStart = 3.5f;
static constexpr float kDT = 0.045f;

static const float kLooks[8][2] = {
    {0.3f, 0.12f}, {0.7f, 0.12f}, {0.3f, 0.38f}, {0.7f, 0.38f},
    {0.3f, 0.62f}, {0.7f, 0.62f}, {0.3f, 0.88f}, {0.7f, 0.88f},
};

static constexpr float kCX = (Hourglass::kWidth / 2.f) * kW;   // 12.9
static constexpr float kCY = (Hourglass::kHeight / 2.f) * kH;  // 12.6
static constexpr int kPc = static_cast<int>(kPX / kW + 0.5f);  // 10
static constexpr int kMid = Hourglass::kWidth / 2;             // 21

static const char* const kPaletteHex[Hourglass::kPaletteN] = {
    "#88a0c0",  // 0 玻璃：冷灰蓝
    "#e0b870",  // 1 沙子：暖琥珀
};

// ============================== 工具函数 ===================================
static inline uint32_t Imul32u(uint32_t a, uint32_t b) { return a * b; }

static float H33(int n) {
  uint32_t h = Imul32u(static_cast<uint32_t>(n) ^ 0x2c1b3c6du, 0x297a2d39u);
  h = Imul32u(h ^ (h >> 15), 0x85ebca77u);
  return static_cast<float>(h ^ (h >> 13)) / 4294967296.0f;
}

static inline float Clamp01(float v) {
  return v < 0.f ? 0.f : v > 1.f ? 1.f : v;
}

template <typename F>
static float Solve(F fn, float want, float a, float b) {
  for (int i = 0; i < 30; ++i) {
    float m = (a + b) * 0.5f;
    if (fn(m) < want)
      a = m;
    else
      b = m;
  }
  return (a + b) * 0.5f;
}

static inline float WOf(float u) {
  float s = std::min(1.f, std::abs(u) / static_cast<float>(kL));
  if (s < 0.76f) {
    float t = std::sin((3.14159265358979323846f / 2.f) * (s / 0.76f));
    return kRN + (kRM - kRN) * std::pow(t, 1.5f);
  }
  return kRM - (kRM - kRC) * std::pow((s - 0.76f) / 0.24f, 2.f);
}

// ============================== 曲线描线 ===================================
void Hourglass::TraceLine(const std::vector<std::pair<float, float>>& pts,
                          const std::function<void(int, int, QChar)>& put) {
  struct Steep {
    int r;
    float x, s;
  };
  std::vector<Steep> steep;

  for (size_t k = 1; k < pts.size(); ++k) {
    float x0 = pts[k - 1].first, y0 = pts[k - 1].second;
    float x1 = pts[k].first, y1 = pts[k].second;
    float dx = x1 - x0, dy = y1 - y0;
    if (dx == 0.f && dy == 0.f)
      continue;

    // 平坦处：每列落一个字符，按穿过格子的高度选字形
    if (std::abs(dy) <= 1.4f * std::abs(dx)) {
      int c_lo = static_cast<int>(std::floor(std::min(x0, x1) / kW - 0.5f)) + 1;
      int c_hi = static_cast<int>(std::floor(std::max(x0, x1) / kW - 0.5f));
      for (int c = c_lo; c <= c_hi; ++c) {
        float y = (y0 + (dy * ((c + 0.5f) * kW - x0)) / dx) / kH;
        int r = static_cast<int>(std::floor(y));
        float f = y - static_cast<float>(r);
        QChar m;
        if (f < 0.3f)
          m = QChar('\'');
        else if (f < 0.6f)
          m = QChar('-');
        else if (f < 0.85f)
          m = QChar('.');
        else
          m = QChar('_');
        put(r, c, m);
      }
    }

    // 陡峭处：每行落一个字符，按斜率选 / \ |，弯曲时用括号
    if (std::abs(dy) >= 0.8f * std::abs(dx)) {
      int r_lo = static_cast<int>(std::floor(std::min(y0, y1) / kH - 0.5f)) + 1;
      int r_hi = static_cast<int>(std::floor(std::max(y0, y1) / kH - 0.5f));
      for (int r = r_lo; r <= r_hi; ++r) {
        float x = x0 + (dx * ((r + 0.5f) * kH - y0)) / dy;
        steep.push_back({r, x, dx / dy});
      }
    }
  }

  for (size_t i = 0; i < steep.size(); ++i) {
    const Steep& st = steep[i];
    QChar m;
    if (std::abs(st.s) > 0.3f)
      m = (st.s > 0) ? QChar('\\') : QChar('/');
    else
      m = QChar('|');

    if (m == QChar('|') && i > 0 && i + 1 < steep.size()) {
      const Steep& a = steep[i - 1];
      const Steep& b = steep[i + 1];
      if (std::abs(a.r - st.r) == 1 && std::abs(b.r - st.r) == 1) {
        float bow = st.x - (a.x + b.x) * 0.5f;
        if (bow > 0.1f)
          m = QChar(')');
        else if (bow < -0.1f)
          m = QChar('(');
      }
    }
    put(st.r, static_cast<int>(std::floor(st.x / kW)), m);
  }
}

// ============================== 球部积分表 =================================
struct BulbEntry {
  float v, lo, hi;
};

static std::vector<BulbEntry> BuildBulb() {
  std::vector<BulbEntry> bulb;
  for (float v = -kRM + 0.05f; v < kRM; v += 0.1f) {
    float lo = static_cast<float>(kL), hi = 0.f;
    for (float u = 0.f; u <= static_cast<float>(kL); u += 0.02f) {
      if (std::abs(v) < WOf(u) - 0.15f) {
        lo = std::min(lo, u);
        hi = u;
      }
    }
    if (hi > lo)
      bulb.push_back({v, lo, hi});
  }
  return bulb;
}

template <typename F>
static float AreaF(const std::vector<BulbEntry>& bulb, F fn) {
  float a = 0.f;
  for (const auto& b : bulb)
    a += fn(b.v, b.lo, b.hi);
  return a;
}

// 一次性把所有沙量查表算好
struct BulbTable {
  std::vector<BulbEntry> bulb;
  std::vector<float> crater;
  std::vector<float> heap;
  float side = 0.f;
  float all = 0.f;
};

static const BulbTable& GetBulbTable() {
  static const BulbTable T = []() {
    BulbTable t;
    t.bulb = BuildBulb();

    auto topArea = [&](float c) {
      return AreaF(t.bulb, [c](float v, float lo, float hi) {
        return std::max(0.f, std::min({hi, kLT, c + kK * std::abs(v)}) - lo);
      });
    };
    auto heapArea = [&](float p) {
      return AreaF(t.bulb, [p](float v, float lo, float hi) {
        return std::max(0.f, hi - std::max(lo, kL - p + kK * std::abs(v)));
      });
    };

    t.all = topArea(kLT);
    for (int i = 0; i <= 50; ++i) {
      t.crater.push_back(
          Solve(topArea, (i / 50.f) * t.all, -kK * kRM - 1.f, kLT));
      t.heap.push_back(Solve(heapArea, (i / 50.f) * t.all, 0.f, kL + kK * kRM));
    }
    t.side = Solve(
        [&](float lv) {
          return AreaF(t.bulb, [lv](float v, float lo, float hi) {
            return v > lv ? hi - lo : 0.f;
          });
        },
        t.all, kRM, -kRM);
    return t;
  }();
  return T;
}

// ============================== 公开接口 ===================================
const char* const* Hourglass::PaletteHex() { return kPaletteHex; }

Hourglass::Hourglass() { Init(); }

// ============================== 静态构建 ===================================
void Hourglass::Init() {
  shells_[0] = MakeShell(false, 0);
  shells_[1] = MakeShell(false, 1);
  shells_[2] = MakeShell(true, 0);

  const int N = kWidth * kHeight;
  chars_.assign(N, QChar(' '));
  colors_.assign(N, 0);
  bits_.assign(N, 0);
}

// Unicode box-drawing 助手
static inline QChar BOX_H() { return QChar(0x2550); }    // ═
static inline QChar BOX_TL() { return QChar(0x2552); }   // ╒
static inline QChar BOX_TR() { return QChar(0x2555); }   // ╕
static inline QChar BOX_BL() { return QChar(0x2558); }   // ╘
static inline QChar BOX_BR() { return QChar(0x255B); }   // ╛
static inline QChar BOX_TT() { return QChar(0x2564); }   // ╤
static inline QChar BOX_BT() { return QChar(0x2567); }   // ╧
static inline QChar BOX_V() { return QChar(0x2502); }    // │
static inline QChar BOX_VR2() { return QChar(0x2562); }  // ╢
static inline QChar BOX_VL2() { return QChar(0x255F); }  // ╟
static inline QChar BOX_DV() { return QChar(0x2551); }   // ║
static inline QChar BOX_TLd() { return QChar(0x2553); }  // ╓
static inline QChar BOX_BLd() { return QChar(0x2559); }  // ╙
static inline QChar BOX_TRd() { return QChar(0x2556); }  // ╖
static inline QChar BOX_BRd() { return QChar(0x255C); }  // ╜
static inline QChar BOX_Hl() { return QChar(0x2500); }   // ─

// ---------- 生成一个玻璃网格 ----------
std::vector<QChar> Hourglass::MakeShell(bool side, int lift) {
  const int N = kWidth * kHeight;
  std::vector<QChar> g(N, QChar(' '));
  std::vector<int8_t> by(N, 0);

  auto put = [&](int r, int c, QChar m) {
    if (r < 0 || r >= kHeight || c < 0 || c >= kWidth)
      return;
    g[r * kWidth + c] = m;
  };

  // 坐标变换：直立时 u 向上，侧翻时 u 向右
  auto at = [&](float u, float v) -> std::pair<float, float> {
    if (side)
      return {kCX + u, kCY + v};
    return {kCX + v, kCY - lift * kH - u};
  };

  // 左右两条壁
  for (int sv : {-1, 1}) {
    std::vector<std::pair<float, float>> pts;
    for (float u = -static_cast<float>(kL);
         u <= static_cast<float>(kL) + 0.001f; u += 0.04f) {
      auto [x, y] = at(u, static_cast<float>(sv) * WOf(u));
      pts.push_back({x, y});
    }
    TraceLine(pts, [&](int r, int c, QChar m) {
      if (r < 0 || r >= kHeight || c < 0 || c >= kWidth)
        return;
      int i = r * kWidth + c;
      ushort u = m.unicode();
      bool flat = (u == '\'' || u == '-' || u == '.' || u == '_');
      // 两条壁同格穿过 → "="
      QChar out = (flat && by[i] == -sv) ? QChar('=') : m;
      g[i] = out;
      if (flat)
        by[i] = (by[i] == -sv) ? 2 : static_cast<int8_t>(sv);
    });
  }

  const int n0 = static_cast<int>(std::round(kCY / kH - 0.5f));

  if (!side) {
    // 上下两块盖板，立柱穿过板
    const int top = n0 - lift - 8;
    const int bot = n0 - lift + 8;
    const int hw = kPc + 2;
    for (int c = kMid - hw; c <= kMid + hw; ++c) {
      const int end = (c == kMid - hw) ? 0 : (c == kMid + hw) ? 1 : -1;
      const bool post = (std::abs(c - kMid) == kPc);

      if (end < 0) {
        put(top - 1, c, BOX_H());
        put(top, c, post ? BOX_TT() : BOX_H());
        put(bot, c, post ? BOX_BT() : BOX_H());
        put(bot + 1, c, BOX_H());
      } else {
        put(top - 1, c, end == 0 ? BOX_TL() : BOX_TR());
        put(top, c, end == 0 ? BOX_BL() : BOX_BR());
        put(bot, c, end == 0 ? BOX_TL() : BOX_TR());
        put(bot + 1, c, end == 0 ? BOX_BL() : BOX_BR());
      }
    }
    for (int r = top + 1; r < bot; ++r) {
      put(r, kMid - kPc, BOX_V());
      put(r, kMid + kPc, BOX_V());
    }
  } else {
    // 侧翻时两端立柱直立、横梁水平
    const int hr = static_cast<int>(std::round(kPX / kH));  // 5
    const int ends[2] = {kMid - 16, kMid + 16};
    for (int k = 0; k < 2; ++k) {
      const int c = ends[k];
      for (int r = n0 - hr - 1; r <= n0 + hr + 1; ++r) {
        const int edge = (r == n0 - hr - 1) ? 0 : (r == n0 + hr + 1) ? 1 : -1;
        const bool post = (std::abs(r - n0) == hr);

        if (edge < 0) {
          // 中间段
          put(r, c - 1, (post && k) ? BOX_VR2() : BOX_DV());
          put(r, c, QChar(' '));
          put(r, c + 1, (post && !k) ? BOX_VL2() : BOX_DV());
        } else {
          // 上/下端点
          put(r, c - 1, edge == 0 ? BOX_TLd() : BOX_BLd());
          put(r, c, BOX_Hl());
          put(r, c + 1, edge == 0 ? BOX_TRd() : BOX_BRd());
        }
      }
    }
    for (int c = ends[0] + 2; c < ends[1] - 1; ++c) {
      put(n0 - hr, c, BOX_Hl());
      put(n0 + hr, c, BOX_Hl());
    }
  }

  return g;
}

// ---------- 每帧 -----------------------------------------------------------
void Hourglass::Update(double time) {
  const float tf = static_cast<float>(time);
  const int N = kWidth * kHeight;
  const BulbTable& T = GetBulbTable();

  // 循环时钟
  float k = std::fmod(kStart + tf, kCycle);
  if (k < 0.f)
    k += kCycle;

  const float turn = k - kRUN - kHOLD;
  int phase;
  if (turn < 0.f)
    phase = -1;
  else if (turn < kTurn[0])
    phase = 0;
  else if (turn < kTurn[0] + kTurn[1])
    phase = 1;
  else
    phase = 2;

  const bool side = (phase == 1);
  const int lift = (phase == 0 || phase == 2) ? 1 : 0;
  const float left = (phase == 2) ? 1.f : Clamp01(1.f - k / kRUN);

  auto tableAt = [](const std::vector<float>& tab, float x) {
    float f = Clamp01(x) * 50.f;
    int i = std::min(49, static_cast<int>(std::floor(f)));
    return tab[i] + (tab[i + 1] - tab[i]) * (f - i);
  };

  const float topC = tableAt(T.crater, left);
  const float heapP = tableAt(T.heap, 1.f - left);

  auto sand = [&](float u, float v) -> bool {
    if (side)
      return u < 0.f && v > T.side;
    if (u > 0.f) {
      return u < std::min(kLT, topC + kK * std::abs(v));
    } else {
      float uu = -u;
      return (static_cast<float>(kL) - uu) < (heapP - kK * std::abs(v));
    }
  };

  // ---------- 每格八次采样 → bits ----------
  std::fill(bits_.begin(), bits_.end(), 0);
  for (int i = 0; i < N; ++i) {
    int hi = 0, lo = 0;
    const int r_i = i / kWidth;
    const int c_i = i % kWidth;
    for (int L = 0; L < 8; ++L) {
      float a = kLooks[L][0], b = kLooks[L][1];
      float x = (c_i + a) * kW - kCX;
      float y = kCY - lift * kH - (r_i + b) * kH;
      float u = side ? x : y;
      float v = side ? -y : x;
      if (std::abs(u) < static_cast<float>(kL) &&
          std::abs(v) < WOf(u) - 0.15f && sand(u, v)) {
        if (b < 0.5f)
          ++hi;
        else
          ++lo;
      }
    }
    bits_[i] = static_cast<uint8_t>((hi > 2 ? 1 : 0) | (lo > 2 ? 2 : 0));
  }

  // ---------- 沙子细流 ----------
  if (phase < 0) {
    const float peak = -static_cast<float>(kL) + heapP;
    int i_start = std::max(0, static_cast<int>(std::floor((k - 0.8f) / kDT)));
    int i_end = static_cast<int>(std::floor(std::min(k, kRUN) / kDT));
    for (int i = i_start; i <= i_end; ++i) {
      float a = k - static_cast<float>(i) * kDT - H33(i + 7919) * kDT * 0.7f;
      float u = 0.1f - 3.f * a - 18.f * a * a;
      if (H33(i) < 0.18f || a < 0.f || u < peak)
        continue;
      float y = (kCY - u) / kH;
      int row = static_cast<int>(std::floor(y));
      if (row < 0 || row >= kHeight)
        continue;
      float frac = y - static_cast<float>(row);
      bits_[row * kWidth + kMid] |= (frac < 0.5f) ? 1 : 2;
    }
  }

  // ---------- 合成 ----------
  // 沙子字符按 bits 选：0 空 / 1 上半 / 2 下半 / 3 都有
  static const QChar kSandChar[4] = {
      QChar(' '),
      QChar('\''),
      QChar('.'),
      QChar(':'),
  };

  const std::vector<QChar>& glass = shells_[side ? 2 : lift];
  for (int i = 0; i < N; ++i) {
    if (glass[i] != QChar(' ')) {
      chars_[i] = glass[i];
      colors_[i] = 0;
    } else if (bits_[i] != 0) {
      chars_[i] = kSandChar[bits_[i]];
      colors_[i] = 1;
    } else {
      chars_[i] = QChar(' ');
      colors_[i] = 0;
    }
  }
}