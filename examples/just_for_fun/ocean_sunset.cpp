#include "ocean_sunset.h"

#include <algorithm>
#include <cmath>

#include <QColor>

// ============================== 常量 =======================================
static constexpr int kHorizon = 56;
static constexpr float kSunX = 134.0f;
static constexpr float kSunY = kHorizon - 4.5f;  // 51.5
static constexpr float kSunR = 8.5f;
static constexpr int kBoatX = 112;

// 风浪：每项 [dir x, dir z, wavenumber, slope, speed, offset]
static const float kChop[4][6] = {
    {-0.45f, 0.89f, 22.f, 0.07f, 2.1f, 0.4f},
    {0.50f, 0.87f, 37.f, 0.06f, 2.9f, 2.1f},
    {-0.15f, 0.99f, 61.f, 0.05f, 3.7f, 4.4f},
    {0.30f, 0.95f, 97.f, 0.04f, 4.6f, 1.3f},
};

// 岬角上的松树：[x, 高度, 底部半宽]
static const float kPines[8][3] = {
    {2.5f, 10.f, 2.6f},  {8.f, 13.f, 3.f},   {13.f, 18.f, 3.6f},
    {18.5f, 11.f, 2.8f}, {24.f, 20.f, 3.8f}, {30.f, 12.f, 2.9f},
    {35.f, 8.f, 2.3f},   {39.5f, 5.f, 1.8f},
};

// 天空渐变，从顶到底，[stop, r, g, b]
static const float kStops[6][4] = {
    {0.00f, 0.08f, 0.10f, 0.30f}, {0.30f, 0.15f, 0.10f, 0.34f},
    {0.52f, 0.40f, 0.15f, 0.40f}, {0.72f, 0.68f, 0.27f, 0.40f},
    {0.88f, 0.80f, 0.32f, 0.40f}, {1.00f, 0.88f, 0.40f, 0.40f},
};

static const char* const kPaletteHex[OceanSunset::kPaletteN] = {
    "#140f2c", "#1e1640", "#2b1c52", "#3d2266", "#552a74", "#6e2c78", "#90327c",
    "#b23c7c", "#d24f7a", "#ea6a78", "#c2306a", "#ff5d8f", "#ff8a9a", "#7a2456",
    "#f05d5e", "#f2804f", "#f99a43", "#ffb44a", "#ffcd62", "#ffe39a", "#fff5d8",
    "#a8323c", "#d2453c", "#e8603e", "#2f1a3e", "#47234f", "#63305c", "#874266",
    "#ac5b70", "#ff9e86", "#ffc4a2", "#ffdcc0", "#0e1230", "#171a46", "#232059",
    "#33286a", "#4a2f78", "#1c2a5c", "#2e4078", "#7d3f8f", "#a24f98", "#c8649e",
    "#de8bb5",
};

// 天空渐变：把 v 落在哪个 stop 段，线性插值出 rgb
static inline void GradientOS(float v, float out[3]) {
  int i = 1;
  while (i < 5 && v > kStops[i][0])
    ++i;
  const float* a = kStops[i - 1];
  const float* b = kStops[i];
  float k = Clampf((v - a[0]) / (b[0] - a[0]));
  out[0] = Mixf(a[1], b[1], k);
  out[1] = Mixf(a[2], b[2], k);
  out[2] = Mixf(a[3], b[3], k);
}

// ============================== 岬角几何 ===================================
// 天空线（岬角顶端）在第 x 列的行号
static inline float RidgeOS(float x) {
  return kHorizon + 2.5f - 11.f * std::pow(Smoothf(54.f, 32.f, x), 0.7f) -
         2.4f * std::exp(-std::pow((x - 19.f) / 9.f, 2.f)) -
         2.f * Fbmf(x * 0.21f, 3.3f, 3, 0);
}
// 岬角山脚与海相接的行号
static inline float ShoreOS(float x) {
  return kHorizon + 1.5f + 5.f * Smoothf(54.f, 4.f, x);
}
// 岬角外的海蚀柱；x 不在范围内返回 1e9（永不遮挡）
static inline float StackOS(float x) {
  if (x > 52.f && x < 58.f) {
    return kHorizon - 3.5f - 1.6f * Fbmf(x * 0.6f, 8.1f, 2, 0) +
           2.5f * std::pow((x - 55.f) / 3.f, 4.f);
  }
  return 1e9f;
}

// ============================== 公开接口 ===================================
const char* const* OceanSunset::PaletteHex() { return kPaletteHex; }

OceanSunset::OceanSunset() { Init(); }

// ============================== 静态构建 ===================================
void OceanSunset::Init() {
  palette_.Init(kPaletteHex, kPaletteN);

  const int N = kWidth * kHeight;

  sky_r_.assign(kHorizon * kWidth, 0.f);
  sky_g_.assign(kHorizon * kWidth, 0.f);
  sky_b_.assign(kHorizon * kWidth, 0.f);
  land_.assign(N, 0);
  land_r_.assign(N, 0.f);
  land_g_.assign(N, 0.f);
  land_b_.assign(N, 0.f);
  deck_.assign(kDeckU * kDeckV, 0.f);
  deck_lit_.assign(kDeckU * kDeckV, 0.f);
  thin_top_.assign(kWidth * kHorizon, 0.f);
  lo_cover_.assign(kLowerW * kHorizon, 0.f);
  lo_lit_.assign(kLowerW * kHorizon, 0.f);

  sk_r_.assign(kHorizon * kWidth, 0.f);
  sk_g_.assign(kHorizon * kWidth, 0.f);
  sk_b_.assign(kHorizon * kWidth, 0.f);
  step_.assign(N, 0);
  color_.assign(N, 0);

  // ---------- 晴空（一次性） ----------
  for (int r = 0; r < kHorizon; ++r) {
    float y = r + 0.5f;
    for (int x = 0; x < kWidth; ++x) {
      float xc = x + 0.5f;
      float dx = xc - kSunX;
      float dy = (y - kSunY) * 1.6f;
      float d = std::sqrt(dx * dx + dy * dy);
      float near = std::exp(-std::abs(dx) / 62.f);
      float v = y / kHorizon;
      float g[3];
      GradientOS(std::pow(v, 1.0f + 0.12f * (1.f - near)), g);

      // 处处玫瑰与紫，只在太阳方向烧成金
      float warm =
          std::exp(-std::abs(dx) / 36.f) * Smoothf(0.45f, 1.f, v) * 0.9f;
      g[0] = Mixf(g[0], 1.0f * Mixf(0.6f, 1.f, v), warm);
      g[1] = Mixf(g[1], 0.64f * Mixf(0.6f, 1.f, v), warm);
      g[2] = Mixf(g[2], 0.3f, warm);

      float fall = Mixf(1.f, 0.86f + 0.14f * near, Smoothf(0.6f, 1.f, v));
      float glow = std::exp(-d / 40.f) * 0.15f + std::exp(-d / 9.f) * 0.1f;
      // 高处薄雾拉成长条，不让任何一片天空是平的
      float haze = 0.82f + 0.36f * Fbmf(x * 0.03f, y * 0.12f, 3, 0);
      int k = r * kWidth + x;
      float vig = (1.f - 0.08f * std::pow(std::abs(xc - 100.f) / 100.f, 2.f)) *
                  haze * fall;
      sky_r_[k] = g[0] * (0.85f + 0.15f * near) * vig + glow;
      sky_g_[k] = g[1] * (0.65f + 0.35f * near) * vig + glow * 0.7f;
      sky_b_[k] = g[2] * (1.1f - 0.2f * near) * vig + glow * 0.3f;
    }
  }

  // ---------- 岬角与松树（一次性） ----------
  // 每行最靠海的岩石列，用来给崖面打光
  float edge[kLandH];
  std::fill(edge, edge + kLandH, -1.f);
  for (int r = 0; r < kLandH; ++r) {
    float y = r + 0.5f;
    for (int x = 0; x < kLandW; ++x) {
      float xc = x + 0.5f;
      if (y >= RidgeOS(xc) && y < ShoreOS(xc))
        edge[r] = static_cast<float>(x);
    }
  }

  for (int r = 0; r < kLandH; ++r) {
    float y = r + 0.5f;
    for (int x = 0; x < kLandW; ++x) {
      int k = r * kWidth + x;
      float xc = x + 0.5f;
      float t0 = RidgeOS(xc);
      float st = StackOS(xc);
      float cr = 0.f, cg = 0.f, cb = 0.f;

      if ((y >= t0 && y < ShoreOS(xc)) || (y >= st && y < kHorizon + 1.2f)) {
        land_[k] = 1;
        bool isStack = !(y >= t0 && y < ShoreOS(xc));
        float top = isStack ? st : t0;
        // 暗紫的岩层，带粗纹理
        float s = 0.6f + 0.8f * Fbmf(x * 0.22f, y * 0.7f, 3, 0);
        cr = 0.06f * s;
        cg = 0.042f * s;
        cb = 0.15f * s;
        // 太阳低而偏右：朝它的坡面接到光
        float facing =
            Clampf(0.4f + (isStack ? 0.5f
                                   : (RidgeOS(xc + 1.2f) - RidgeOS(xc - 1.2f)) *
                                         0.45f));
        float rim = Smoothf(top + 2.4f, top + 0.4f, y) * facing;
        // 崖面：转向太阳的一侧偏暖，被巉岩打断
        float crag =
            Smoothf(0.35f, 0.75f, Fbmf(x * 0.4f + 3.f, y * 0.5f, 3, 0));
        float face;
        if (isStack) {
          face = Smoothf(53.5f, 57.5f, xc) * 0.7f;
        } else if (edge[r] > 30.f) {
          face = std::exp(-(edge[r] - x) / 4.f) *
                 Smoothf(kHorizon + 5.f, kHorizon - 4.f, y);
        } else {
          face = 0.f;
        }
        float lit =
            Clampf(std::max(rim * 0.9f, face * (0.3f + 0.7f * crag) * 0.75f));
        cr = Mixf(cr, 0.95f, lit);
        cg = Mixf(cg, 0.38f, lit * 0.95f);
        cb = Mixf(cb, 0.30f, lit * 0.9f);
      }

      for (int pi = 0; pi < 8; ++pi) {
        float tx = kPines[pi][0], th = kPines[pi][1], tw = kPines[pi][2];
        float tb = RidgeOS(tx);
        float dy = y - (tb - th);
        // 针叶树：尖塔形，枝条一层层向外张开
        float tier = (dy + th * 0.3f) / 2.4f;
        float half =
            (dy / th) * tw * (0.5f + 0.75f * (tier - std::floor(tier))) + 0.35f;
        float ex = xc - tx;
        if (dy >= 0.f && y < tb + 1.5f && std::abs(ex) <= half) {
          land_[k] = 2;
          float s = 0.7f + 0.6f * Hashf(x * 13 + r, 5);
          cr = 0.045f * s;
          cg = 0.03f * s;
          cb = 0.1f * s;
          // 右翼朝太阳
          float e = (ex > 0.f && ex > half - 1.1f)
                        ? 0.3f + 0.3f * Smoothf(th, 0.f, dy)
                        : 0.f;
          cr = Mixf(cr, 0.85f, e);
          cg = Mixf(cg, 0.30f, e);
          cb = Mixf(cb, 0.30f, e);
        }
      }

      if (land_[k]) {
        land_r_[k] = cr;
        land_g_[k] = cg;
        land_b_[k] = cb;
      }
    }
  }

  // 一艘小帆船停在太阳左侧的辉光里，暗影映在水上
  for (int r = kHorizon - 10; r < kHorizon + 4; ++r) {
    float y = r + 0.5f;
    for (int x = kBoatX - 6; x <= kBoatX + 6; ++x) {
      int k = r * kWidth + x;
      float ex = x + 0.5f - kBoatX;
      bool hull = (y >= kHorizon + 1 && y < kHorizon + 3 &&
                   std::abs(ex - 0.3f) < 4.6f - (y - kHorizon - 1.f) * 1.3f);
      bool mast =
          (std::abs(ex) < 0.5f && y >= kHorizon - 9 && y < kHorizon + 1);
      bool main = (ex > 0 && y >= kHorizon - 8.5f && y < kHorizon + 0.5f &&
                   ex < 0.6f + (y - (kHorizon - 8.5f)) * 0.42f);
      bool jib = (ex < 0 && y >= kHorizon - 7.f && y < kHorizon + 0.5f &&
                  -ex < (y - (kHorizon - 7.f)) * 0.36f);
      if (hull || mast || main || jib) {
        land_[k] = 3;
        // 帆很薄，背后阳光透过来会亮一点
        float glow = main ? 0.12f + 0.18f * Smoothf(0.f, 3.5f, ex) : 0.f;
        land_r_[k] = 0.05f + glow;
        land_g_[k] = 0.03f + glow * 0.45f;
        land_b_[k] = 0.09f + glow * 0.35f;
      }
    }
  }

  // ---------- 云顶：从下往上看的堆云，铺在自己的平面上，
  //            越靠地平线越缩小、越压平 -------------
  auto deckAt = [](float u, float v) {
    float q = Fbmf(u * 0.0125f, v * 0.06f, 2, 8);
    float patch = Fbmf(u * 0.00625f, v * 0.03f + 7.f, 2, 4);
    return Fbmf(u * 0.040625f + q * 1.6f, v * 0.3f, 5, 26) +
           0.25f * (patch - 0.5f);
  };
  for (int v = 0; v < kDeckV; ++v) {
    for (int u = 0; u < kDeckU; ++u) {
      float d = deckAt(static_cast<float>(u), static_cast<float>(v));
      deck_[v * kDeckU + u] = d;
      // 每堆朝地平线、朝太阳的一面被照亮
      deck_lit_[v * kDeckU + u] =
          Clampf(0.5f + (d - deckAt(static_cast<float>(u), v + 2.5f)) * 9.f);
    }
  }
  // 往天顶方向变薄，边缘参差而不是一条直边
  for (int r = 0; r < kHorizon; ++r) {
    for (int x = 0; x < kWidth; ++x) {
      thin_top_[r * kWidth + x] = Fbmf(x * 0.05f + 11.f, r * 0.15f, 2, 0);
    }
  }

  // ---------- 地平线上的低云带：可环绕、随帧漂移 ----------
  auto lower = [](float x, float y) {
    // 一条条细长的低云
    float b = Fbmf(x / 40.f, y * 0.3f, 4, kLowerW / 40);
    float gaps = Fbmf(x / 120.f, 5.f, 2, kLowerW / 120);
    return b + 0.01f + 0.5f * (gaps - 0.5f) -
           0.35f * Smoothf(kHorizon - 14.f, kHorizon - 20.f, y) -
           0.25f * Smoothf(kHorizon - 4.f, kHorizon - 1.f, y);
  };
  for (int r = 0; r < kHorizon; ++r) {
    float y = r + 0.5f;
    for (int x = 0; x < kLowerW; ++x) {
      float dl = lower(static_cast<float>(x), y);
      lo_cover_[r * kLowerW + x] = Smoothf(0.58f, 0.66f, dl);
      lo_lit_[r * kLowerW + x] =
          Clampf(0.5f + (dl - lower(static_cast<float>(x), y + 1.2f)) * 6.f);
    }
  }
}

// ---------- 单格覆盖 -------------------------------------------------------
void OceanSunset::Halftone(int k, int r, int x, float cr, float cg, float cb,
                           float base_level, float fade) {
  float peak = std::max({cr, cg, cb, 1e-4f});
  float level =
      Clampf(base_level + (1.f - base_level) * std::pow(peak, 0.85f) * 0.95f) *
      fade;
  int step = std::max(
      0,
      std::min(3, static_cast<int>(std::round(
                      level * 3.f + kHalftoneBayer[(r & 3) * 4 + (x & 3)]))));
  step_[k] = static_cast<uint8_t>(step);
  if (step) {
    // 小点画得更亮来补偿面积，但最暗的格子保持暗淡的颜色，
    // 让阴影保持深沉而不闪光
    float want = std::min(1.f, (level + 0.06f) / kHalftoneCover[step]);
    float s =
        ((0.3f + 0.7f * want) * (0.4f + 0.6f * Smoothf(0.14f, 0.5f, level))) /
        peak;
    color_[k] = static_cast<uint8_t>(
        palette_.Nearest(Clampf(cr * s), Clampf(cg * s), Clampf(cb * s)));
  } else {
    color_[k] = 0;
  }
}

// ---------- 每帧 -----------------------------------------------------------
void OceanSunset::Update(double t) {
  const float tf = static_cast<float>(t);
  const float dUp = tf * 0.9f;
  const float dLo = tf * 1.7f;
  const int N = kWidth * kHeight;

  // ---------- 天空 ----------
  for (int r = 0; r < kHorizon; ++r) {
    float y = r + 0.5f;
    for (int x = 0; x < kWidth; ++x) {
      int k = r * kWidth + x;
      float cr = sky_r_[k], cg = sky_g_[k], cb = sky_b_[k];

      float dx = x + 0.5f - kSunX;
      float dy = y - kSunY;
      float disc = 0.f;
      float ds = std::sqrt(dx * dx + std::pow(dy / 0.9f, 2.f));
      if (ds < kSunR + 0.6f) {
        // 太阳圆盘，略扁，中心炽热，边缘更红
        float e = ds / kSunR;
        disc = Smoothf(kSunR + 0.45f, kSunR - 0.45f, ds);
        float e4 = e * e * e * e;
        cr = Mixf(cr, 1.0f, disc);
        cg = Mixf(cg, 0.95f - 0.2f * e4, disc);
        cb = Mixf(cb, 0.78f - 0.38f * e4, disc);
      }

      float near = std::exp(-std::sqrt(dx * dx + dy * dy * 2.5f) / 45.f);
      float v = y / kHorizon;

      // 云顶：在自己的平面上找位置，越远的行距离越远
      float D = 58.f / (kHorizon - y + 2.5f);
      // 中天一堆堆的积云：上方和地平线低处的辉光都让开
      float far = Smoothf(4.8f, 2.7f, D) * Smoothf(1.08f, 1.7f, D);
      float cloud = 0.f;
      if (far > 0.f) {
        float su = dx * D + dUp + 64200.f;
        float sv = (D - 0.9f) * 10.f;
        float iu_f = std::floor(su);
        float fu = su - iu_f;
        float iv_f = std::floor(sv);
        float fv = sv - iv_f;
        int iu = static_cast<int>(iu_f) % kDeckU;
        if (iu < 0)
          iu += kDeckU;
        int iv = static_cast<int>(iv_f);
        int iu1 = (iu + 1) % kDeckU;
        int a0 = iv * kDeckU;
        int a1 = a0 + kDeckU;
        if (a0 >= 0 && a1 + iu1 < kDeckU * kDeckV) {
          float d0 = deck_[a0 + iu] + (deck_[a0 + iu1] - deck_[a0 + iu]) * fu;
          float d1 = deck_[a1 + iu] + (deck_[a1 + iu1] - deck_[a1 + iu]) * fu;
          float dd = d0 + (d1 - d0) * fv - 0.22f * (1.f - far) * thin_top_[k];
          float c = Smoothf(0.56f, 0.68f, dd) * std::sqrt(far);
          if (c > 0.01f) {
            cloud = c;
            float l0 = deck_lit_[a0 + iu] +
                       (deck_lit_[a0 + iu1] - deck_lit_[a0 + iu]) * fu;
            float l1 = deck_lit_[a1 + iu] +
                       (deck_lit_[a1 + iu1] - deck_lit_[a1 + iu]) * fu;
            float l = l0 + (l1 - l0) * fv;
            // 云底：朝太阳一侧偏金，背离一侧偏玫瑰；位置越低越明显；
            // 厚厚的云心保持黄昏的紫罗兰
            float az = std::exp(-std::abs(dx) / 60.f);
            float low = v * v;
            float thin = 1.f - Smoothf(0.6f, 0.76f, dd);
            float b = Clampf(std::pow(Smoothf(0.45f, 0.85f, l), 1.5f) *
                                 (0.55f + 0.25f * low + 0.35f * az) +
                             0.3f * thin * az * low);
            float warm = Clampf(az * (0.1f + 1.2f * low));
            float hr = 1.0f;
            float hg = Mixf(0.45f, 0.72f, warm);
            float hb = Mixf(0.52f, 0.38f, warm);
            float sr = Mixf(0.10f, 0.20f, v);
            float sg = Mixf(0.065f, 0.07f, v);
            float sb = Mixf(0.23f, 0.27f, v);
            cr = Mixf(cr, Mixf(sr, hr, b), c);
            cg = Mixf(cg, Mixf(sg, hg, b), c);
            cb = Mixf(cb, Mixf(sb, hb, b), c);
          }
        }
      }

      // 低云带：在辉光前是暗的，边缘烧得发亮
      float sx = x + dLo;
      float ix_f = std::floor(sx);
      float fxl = sx - ix_f;
      int ixl = static_cast<int>(ix_f) % kLowerW;
      if (ixl < 0)
        ixl += kLowerW;
      int ixl1 = (ixl + 1) % kLowerW;
      int i0l = r * kLowerW + ixl;
      int i1l = r * kLowerW + ixl1;
      // 在太阳上方变薄，岬角背后的天空不要
      float cl = (lo_cover_[i0l] + (lo_cover_[i1l] - lo_cover_[i0l]) * fxl) *
                 (1.f - 0.75f * std::exp(-std::pow(dx / 13.f, 2.f))) *
                 (1.f - 0.85f * Smoothf(76.f, 50.f, static_cast<float>(x)));
      if (cl > 0.01f) {
        cloud = std::max(cloud, cl);
        float l = lo_lit_[i0l] + (lo_lit_[i1l] - lo_lit_[i0l]) * fxl;
        float rim = std::pow(l, 2.f) * (0.35f + 0.9f * near);
        float a = std::min(1.f, cl * 1.2f) * 0.92f;
        cr = Mixf(cr, 0.30f + 0.7f * rim, a);
        cg = Mixf(cg, 0.09f + 0.55f * rim, a);
        cb = Mixf(cb, 0.24f + 0.18f * rim, a);
      }

      // 天上最先亮起的几颗星，在高处的靛蓝里
      if (r < 22 && cloud < 0.05f && Hashf(x, r * 5 + 3) > 0.993f) {
        float tw = 0.55f + 0.45f * std::sin(tf * (0.8f + Hashf(r, x) * 1.6f) +
                                            Hashf(x, r) * 6.28f);
        float st = tw * Smoothf(22.f, 4.f, static_cast<float>(r)) * 0.75f;
        cr = std::max(cr, st * 0.95f);
        cg = std::max(cg, st * 0.85f);
        cb = std::max(cb, st);
      }

      // 海太粗糙，照不出完整的日盘，只能还回闪砾：镜子层抹掉日盘
      float keep = 1.f - 0.75f * disc;
      sk_r_[k] = cr * keep;
      sk_g_[k] = cg * keep;
      sk_b_[k] = cb * keep;

      if (land_[k]) {
        Halftone(k, r, x, land_r_[k], land_g_[k], land_b_[k], 0.02f, 1.f);
      } else {
        Halftone(k, r, x, cr, cg, cb, 0.05f, 1.f);
      }
    }
  }

  // ---------- 海面 ----------
  for (int r = kHorizon; r < kHeight; ++r) {
    float y = r + 0.5f;
    // 每格是一小片水面，按它自身的倾角去镜像天空的某一处
    float dz = y - kHorizon;
    float Z = 36.f / dz;
    float rows = 36.f / (dz * dz);
    float swellAmt = Smoothf(8.f, 26.f, dz);
    float hot = std::exp(-dz / 9.f);
    float pw = 3.f + dz * 0.8f;  // 闪砾光路，越靠前越宽
    float fxc = (0.9f / (1.f + dz * 0.06f)) * 0.35f;
    float fyc = 1.6f / (1.f + dz * 0.05f);
    float fres = 0.24f + 0.46f * std::exp(-dz / 8.f);
    float depth = Smoothf(kHorizon, kHeight, y);
    float fade = Smoothf(kHeight + 6.f, kHeight - 4.f, y);

    for (int x = 0; x < kWidth; ++x) {
      int k = r * kWidth + x;
      if (land_[k]) {
        Halftone(k, r, x, land_r_[k], land_g_[k], land_b_[k], 0.02f, 1.f);
        continue;
      }

      float dx = x + 0.5f - kSunX;
      float X = (x + 0.5f - 100.f) / dz;

      // 长涌浪朝我们滚来，短风浪骑在上面
      float p = 13.f * (Z + 0.05f * X) +
                2.2f * Fbmf(X * 0.35f + 3.f, Z * 0.4f, 2, 0) + tf * 0.8f;
      float wv = std::sin(p), slope = std::cos(p);
      float sz = 0.11f * slope * Clampf(1.4f / (13.f * rows));
      float sxl = 0.015f * slope;
      for (int i = 0; i < 4; ++i) {
        float kx = kChop[i][0], kz = kChop[i][1], kk = kChop[i][2];
        float a = kChop[i][3], w = kChop[i][4], f = kChop[i][5];
        float ph = kk * (kx * X + kz * Z) - w * tf + f;
        float c = std::cos(ph) * a * Clampf(1.6f / (kk * rows));
        sz += c * kz;
        sxl += c * kx;
      }
      int ry =
          static_cast<int>(std::round(kHorizon - 1.f - dz * 0.85f - 70.f * sz));
      ry = std::max(0, std::min(kHorizon - 1, ry));
      int rx = static_cast<int>(std::round(x - 60.f * sxl));
      rx = std::max(0, std::min(kWidth - 1, rx));
      int q = ry * kWidth + rx;
      // 地平线附近偏紫，往我们这边渐深成靛蓝
      // 破成长长的水平波纹，每一道还回或多或少的天空
      float rip = Noisef(x * fxc * 1.4f + 13.f - tf * 0.15f,
                         y * fyc * 1.1f + tf * 0.25f, 0);
      float rf = fres * (0.5f + 1.1f * rip * rip);
      float cr = sk_r_[q] * rf * 0.8f + 0.03f - 0.01f * depth;
      float cg = sk_g_[q] * rf * 0.75f + 0.022f;
      float cb = sk_b_[q] * rf + 0.095f + 0.03f * depth;

      // 波纹小面：远处的短、近处的长，轮流朝太阳转过去又转开
      float n =
          0.55f * Noisef(x * fxc + tf * 0.3f, y * fyc * 1.3f - tf * 0.4f, 0) +
          0.45f * Noisef(x * fxc * 1.7f - tf * 0.4f,
                         y * fyc * 2.f + tf * 0.3f + 40.f, 0);
      int wave = static_cast<int>(std::floor(p / (2.f * 3.14159265f)));
      float crest =
          std::pow(0.5f + 0.5f * wv, 7.f) * swellAmt *
          Smoothf(0.4f, 0.65f, Noisef(X * 2.5f + 7.f, wave * 3.7f, 0));
      float path = std::exp(-std::pow(dx / pw, 2.f));
      float soft = std::exp(-std::pow(dx / (pw * 1.6f), 2.f));
      // 涌浪的近侧坡面在阴影里
      if (slope > 0) {
        float d = 1.f - 0.7f * swellAmt * slope;
        cr *= d;
        cg *= d;
        cb *= d;
      }
      float th = 0.84f - 0.3f * path * (0.45f + 0.55f * hot);
      float glint = Smoothf(th, th + 0.08f, n) * (0.25f + 0.75f * path) * soft;
      float white = path * path * Smoothf(th + 0.04f, th + 0.2f, n);
      cr += 0.20f * soft * (0.4f + hot) + glint * 1.1f;
      cg += 0.10f * soft * (0.4f + hot) +
            glint * (0.55f + 0.3f * hot + 0.35f * white);
      cb += 0.04f * soft + glint * (0.2f + 0.25f * hot + 0.45f * white);
      // 涌浪顶接住它：光路里偏金，两侧偏玫瑰
      float catchL = crest * (0.15f + 0.85f * soft) * (0.6f + 0.6f * n);
      cr += catchL * 0.95f;
      cg += catchL * Mixf(0.3f, 0.7f, soft);
      cb += catchL * Mixf(0.5f, 0.3f, soft);
      // 太阳自己的光柱：日盘下方先空一段，然后是断断续续的短划
      float dash = Smoothf(
          0.4f, 0.7f, Noisef(x * 0.18f + tf * 0.25f, y * 0.9f - tf * 0.5f, 0));
      float col = std::exp(-std::pow(dx / (1.f + dz * 0.22f), 2.f)) *
                  std::exp(-dz / 5.f) * (0.6f + 0.4f * n) *
                  Smoothf(0.f, 3.f, dz) * dash;
      cr += col;
      cg += col * 0.8f;
      cb += col * 0.45f;

      // 水里的岬角倒影，被波纹打断
      if (x < kLandW + 4 && dz < 26.f) {
        float xs = x + 0.5f + 1.3f * std::sin(y * 1.1f + tf * 1.2f + x * 0.05f);
        int ixl2 = std::max(0, std::min(kLandW - 1, static_cast<int>(xs)));
        float ft = ShoreOS(ixl2 + 0.5f);
        if (y > ft) {
          int my = static_cast<int>(std::floor(2.f * ft - y));
          if (my >= 0 && my < kLandH) {
            int m = my * kWidth + ixl2;
            if (land_[m]) {
              float a = 0.8f * Smoothf(ft + 24.f, ft + 4.f, y);
              cr = Mixf(cr, land_r_[m] * 0.8f + 0.02f, a);
              cg = Mixf(cg, land_g_[m] * 0.7f + 0.015f, a);
              cb = Mixf(cb, land_b_[m] * 0.85f + 0.05f, a);
            }
          }
          // 岩与水相交处的一条浪花
          float foam =
              Smoothf(ft + 1.6f, ft + 0.3f, y) *
              Smoothf(0.45f, 0.8f, Noisef(x * 0.5f - tf * 0.4f, tf * 0.3f, 0));
          cr += 0.5f * foam;
          cg += 0.3f * foam;
          cb += 0.35f * foam;
        }
      }
      // 帆船的倒影，更短、更碎
      if (dz > 2.5f && dz < 14.f && x > kBoatX - 8 && x < kBoatX + 8) {
        int ixb =
            static_cast<int>(x + 0.5f + 0.9f * std::sin(y * 1.3f + tf * 1.6f));
        int m =
            static_cast<int>(std::floor(2.f * (kHorizon + 3) - y)) * kWidth +
            ixb;
        if (m >= 0 && m < N && land_[m] == 3) {
          float a = 0.7f * Smoothf(kHorizon + 14.f, kHorizon + 4.f, y) *
                    (0.6f + 0.4f * rip);
          cr = Mixf(cr, 0.04f, a);
          cg = Mixf(cg, 0.03f, a);
          cb = Mixf(cb, 0.10f, a);
        }
      }
      // 地平线一条清晰的暗线，让太阳坐上去
      if (dz < 1.5f) {
        cr *= 0.55f;
        cg *= 0.55f;
        cb *= 0.55f;
      }

      Halftone(k, r, x, cr, cg, cb, 0.18f, fade);
    }
  }
}