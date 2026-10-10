// ============================================================================
//  halftone_common — 五个半色调场景共用的工具与常量
//
//  共享内容：
//    - Imul32 / Hashf / Noisef / Fbmf   噪声与哈希
//    - Clampf / Smoothf / Mixf          插值
//    - kHalftoneCover / kHalftoneBayer  抖动与覆盖
//    - HalftonePalette                  调色板初始化 + 最近色 LUT
//
//  Fbmf 的 octaveOffset 参数：
//    desert_night 在原 TS 里每个八度都有 + i * 31.7 的偏移，其他场景没有。
//    默认 0，desert_night 显式传 31.7f 保持原行为。
// ============================================================================
#ifndef EXAMPLES_JUST_FOR_FUN_HALFTONE_COMMON_H_
#define EXAMPLES_JUST_FOR_FUN_HALFTONE_COMMON_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include <QColor>

// ============================== 常量 =======================================
static constexpr float kHalftoneCover[4] = {0.0f, 0.3f, 0.6f, 1.0f};
static constexpr float kHalftoneBayer[16] = {
    0.0f / 16 - 0.47f,  8.0f / 16 - 0.47f,  2.0f / 16 - 0.47f,
    10.0f / 16 - 0.47f, 12.0f / 16 - 0.47f, 4.0f / 16 - 0.47f,
    14.0f / 16 - 0.47f, 6.0f / 16 - 0.47f,  3.0f / 16 - 0.47f,
    11.0f / 16 - 0.47f, 1.0f / 16 - 0.47f,  9.0f / 16 - 0.47f,
    15.0f / 16 - 0.47f, 7.0f / 16 - 0.47f,  13.0f / 16 - 0.47f,
    5.0f / 16 - 0.47f,
};

// ============================== 工具函数 ===================================
inline uint32_t Imul32(uint32_t a, uint32_t b) {
  return static_cast<uint32_t>(
      (static_cast<uint64_t>(a) * static_cast<uint64_t>(b)));
}
inline float Hashf(int x, int y) {
  uint32_t h = Imul32(static_cast<uint32_t>(x), 374761393u) +
               Imul32(static_cast<uint32_t>(y), 668265263u);
  h = Imul32(h ^ (h >> 13), 1274126177u);
  h = h ^ (h >> 16);
  return static_cast<float>((h / 4294967296.0));
}
inline float Noisef(float x, float y, int period) {
  float xi = std::floor(x), yi = std::floor(y);
  float fx = x - xi, fy = y - yi;
  float u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
  int x0 = static_cast<int>(xi), x1 = x0 + 1;
  if (period) {
    x0 = ((x0 % period) + period) % period;
    x1 = (x0 + 1) % period;
  }
  int y0 = static_cast<int>(yi), y1 = y0 + 1;
  float a = Hashf(x0, y0), b = Hashf(x1, y0), c = Hashf(x0, y1),
        d = Hashf(x1, y1);
  return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
inline float Fbmf(float x, float y, int oct, int period,
                  float octaveOffset = 0.f) {
  float s = 0, n = 0, amp = 0.5f, f = 1;
  for (int i = 0; i < oct; ++i) {
    s += amp * Noisef(x * f + octaveOffset * static_cast<float>(i), y * f,
                      static_cast<int>((period * f)));
    n += amp;
    amp *= 0.5f;
    f *= 2;
  }
  return s / n;
}
inline float Clampf(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
inline float Smoothf(float a, float b, float v) {
  float k = Clampf((v - a) / (b - a));
  return k * k * (3 - 2 * k);
}
inline float Mixf(float a, float b, float k) { return a + (b - a) * k; }

// ============================== 调色板 =====================================
// 负责：初始化 palette_ 与 lut_，把 (r,g,b) 量化到最近色索引。
class HalftonePalette {
 public:
  void Init(const char* const* hex, int n) {
    colors_.resize(n);
    for (int i = 0; i < n; ++i) {
      QColor c(hex[i]);
      colors_[i][0] = c.redF();
      colors_[i][1] = c.greenF();
      colors_[i][2] = c.blueF();
    }
    lut_.assign(32768, 255);
  }

  int size() const { return static_cast<int>(colors_.size()); }
  const std::array<float, 3>& color(int i) const { return colors_[i]; }

  int Nearest(float r, float g, float b) {
    int ri = std::min(31, static_cast<int>((r * 31.99f)));
    int gi = std::min(31, static_cast<int>((g * 31.99f)));
    int bi = std::min(31, static_cast<int>((b * 31.99f)));
    int key = (ri << 10) | (gi << 5) | bi;
    if (lut_[key] != 255)
      return lut_[key];
    int best = 0;
    float bd = 1e9f;
    for (int i = 0; i < static_cast<int>(colors_.size()); ++i) {
      float dr = colors_[i][0] - r, dg = colors_[i][1] - g,
            db = colors_[i][2] - b;
      float d = 0.3f * dr * dr + 0.5f * dg * dg + 0.2f * db * db;
      if (d < bd) {
        bd = d;
        best = i;
      }
    }
    return lut_[key] = static_cast<uint8_t>(best);
  }

 private:
  std::vector<std::array<float, 3>> colors_;
  std::vector<uint8_t> lut_;
};

#endif  // EXAMPLES_JUST_FOR_FUN_HALFTONE_COMMON_H_