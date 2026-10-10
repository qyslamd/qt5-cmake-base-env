#include "alpine_dawn.h"

#include <algorithm>
#include <cmath>

#include <QColor>
#include <QtMath>

// ============================== 常量 =======================================
static constexpr float kFovTan = 0.62f;    // 视场半角的切线，横向
static constexpr float kEyeRow = 56.5f;    // 眼高（行）
static constexpr float kCamHeight = 1.5f;  // 相机在水面上的高度
static constexpr float kShoreZ = 46.0f;    // 远岸距离
static constexpr float kSunX = 151.0f;
static constexpr float kSunY = 50.0f;

// 归一化的光照方向（原 TS: [0.9, 0.3, 0.14] / norm）
static const float kLightX = 0.9385f;
static const float kLightY = 0.3128f;
static const float kLightZ = 0.1460f;

static const char* const kPaletteHex[AlpineDawn::kPaletteN] = {
    "#0e1430", "#151d40", "#1d2752", "#263365", "#314179", "#3e508c", "#4f62a0",
    "#6577b3", "#8090c4", "#9eaad3", "#bec6e2", "#4b3e6c", "#6a5482", "#8c6a92",
    "#b0829c", "#cf96a4", "#e8a9a8", "#f5bcaa", "#ffd0b0", "#ffe2c2", "#fff1e0",
    "#fdfaf6", "#ffc887", "#f7a965", "#f2a08f", "#e58a87", "#f8b59d", "#d97b7e",
    "#7a4c4a", "#a5654f", "#523a4a", "#1b2034", "#262c45", "#363c59", "#0a1418",
    "#0f1f24", "#162a2f", "#203a3c", "#a3a7c6", "#c6c3d8", "#e0d4dc", "#5a4f7e",
    "#7b6c9c", "#9a8cb6", "#b8a8c8", "#d8bccb",
};

// 尖锐山脊：普通噪声越过中线时形成的棱
static inline float Ridged(float x, float y, int oct) {
  float s = 0, n = 0, amp = 0.5f, f = 1;
  for (int i = 0; i < oct; ++i) {
    float v = 1 - std::fabs(2 * Noisef(x * f + i * 17.3f, y * f, 0) - 1);
    s += amp * v * v;
    n += amp;
    amp *= 0.5f;
    f *= 2.1f;
  }
  return s / n;
}

// ============================== 山峰 =======================================
struct Peak {
  float px, pz, ph, pr, c, s;
};

static std::array<Peak, 7> BuildPeaks() {
  // 原始参数 [屏幕列, 屏幕行, 距离, 展开, 转角]
  static const float kRawPeaks[7][5] = {
      {66, 11, 140, 0.95f, 0.30f},  {38, 26, 115, 1.10f, -0.15f},
      {116, 22, 175, 0.90f, 0.20f}, {92, 33, 150, 1.00f, 0.10f},
      {96, 25, 340, 1.10f, 0.40f},  {180, 38, 200, 1.50f, -0.10f},
      {8, 30, 160, 1.20f, 0.25f},
  };
  std::array<Peak, 7> out;
  for (int i = 0; i < 7; ++i) {
    float sx = kRawPeaks[i][0];
    float row = kRawPeaks[i][1];
    float z = kRawPeaks[i][2];
    float f = kRawPeaks[i][3];
    float a = kRawPeaks[i][4];
    float h = kCamHeight + ((kEyeRow - row) / 100.0f) * kFovTan * z - 1.5f;
    out[i].px = ((sx - 100.0f) / 100.0f) * kFovTan * z;
    out[i].pz = z;
    out[i].ph = h;
    out[i].pr = h * f;
    out[i].c = std::cos(a);
    out[i].s = std::sin(a);
  }
  return out;
}
static const std::array<Peak, 7>& Peaks() {
  static const std::array<Peak, 7> kPeaks = BuildPeaks();
  return kPeaks;
}

static float TerrainAD(float x, float z) {
  float h = 0;
  for (const auto& p : Peaks()) {
    float dx = x - p.px, dz = z - p.pz;
    float rx = dx * p.c - dz * p.s;
    float rz = dx * p.s + dz * p.c;
    float v = p.ph * (1 - (std::fabs(rx) + std::fabs(rz)) / p.pr);
    if (v > h)
      h = v;
  }
  float hills = (1 + 3 * Fbmf(x * 0.04f, z * 0.04f, 3, 0)) *
                Smoothf(kShoreZ, kShoreZ + 15, z);
  if (hills > h)
    h = hills;
  h += ((Ridged(x * 0.06f, z * 0.06f, 3) - 0.45f) * 6 +
        (Ridged(x * 0.20f, z * 0.20f, 2) - 0.45f) * 1.6f) *
       Smoothf(4, 22, h);
  return h;
}

// 沿射线（斜率 v，横比 u，相机高 oy）步进到地形，返回距离（0 = 天空）
static float MarchAD(float u, float v, float oy) {
  float z = kShoreZ, prev = z;
  for (int i = 0; i < 260 && z < 520; ++i) {
    float gap = oy + v * z - TerrainAD(u * z, z);
    if (gap < 0) {
      float a = prev, b = z;
      for (int j = 0; j < 7; ++j) {
        float m = (a + b) * 0.5f;
        if (oy + v * m - TerrainAD(u * m, m) < 0)
          b = m;
        else
          a = m;
      }
      return b;
    }
    prev = z;
    z += std::max(0.35f, gap * 0.45f) + z * 0.002f;
  }
  return 0;
}

// ============================== 公开接口 ===================================
const char* const* AlpineDawn::PaletteHex() { return kPaletteHex; }
AlpineDawn::AlpineDawn() { Init(); }

// ============================== 静态构建 ===================================
void AlpineDawn::Init() {
  const int N = kWidth * kHeight;

  palette_.Init(kPaletteHex, kPaletteN);

  depth_.assign(kShoreRow * kWidth, 0);
  alt_.assign(kShoreRow * kWidth, 0);
  sun_.assign(kShoreRow * kWidth, 0);
  snow_.assign(kShoreRow * kWidth, 0);
  up_.assign(kShoreRow * kWidth, 0);
  rim_.assign(kShoreRow * kWidth, 0);
  tree_top_.assign(kWidth, 0);
  src_.assign(kLakeRow * kWidth, 0);
  fg_.assign(N, 0);
  fg_shade_.assign(N, 0);
  fg_rim_.assign(N, 0);
  mist_.assign(kMistW * kMistRows, 0);
  cloud_.assign(kCloudW * kCloudRows, 0);
  grain_.assign(N, 0);
  sky_r_.assign(kShoreRow * kWidth, 0);
  sky_g_.assign(kShoreRow * kWidth, 0);
  sky_b_.assign(kShoreRow * kWidth, 0);
  glow_a_.assign(kShoreRow * kWidth, 0);

  a_r_.assign(kShoreRow * kWidth, 0);
  a_g_.assign(kShoreRow * kWidth, 0);
  a_b_.assign(kShoreRow * kWidth, 0);
  f_r_.assign(N, 0);
  f_g_.assign(N, 0);
  f_b_.assign(N, 0);
  flo_.assign(N, 0);
  fade_.assign(N, 1);
  wisp_.assign(kWidth, 0);
  step_.assign(N, 0);
  color_.assign(N, 0);

  // ---------- 山脉：一次性 raymarch ----------
  for (int r = 0; r < kShoreRow; ++r) {
    float v = ((kEyeRow - (r + 0.5f)) / 100.0f) * kFovTan;
    for (int x = 0; x < kWidth; ++x) {
      float u = ((x + 0.5f - 100.0f) / 100.0f) * kFovTan;
      float z = MarchAD(u, v, kCamHeight);
      int k = r * kWidth + x;
      if (z == 0)
        continue;
      float px = u * z, py = kCamHeight + v * z;
      const float e = 0.35f;
      float hx = (TerrainAD(px + e, z) - TerrainAD(px - e, z)) / (2 * e);
      float hz = (TerrainAD(px, z + e) - TerrainAD(px, z - e)) / (2 * e);
      float nl = std::sqrt(hx * hx + 1 + hz * hz);
      float nx = -hx / nl, ny = 1.0f / nl, nz = -hz / nl;
      float lit = std::max(0.f, nx * kLightX + ny * kLightY + nz * kLightZ);
      if (lit > 0) {
        for (float s = 0.8f; s < 160; s += 0.6f + s * 0.04f) {
          float qx = px + kLightX * s;
          float qy = py + kLightY * s + 0.15f;
          float qz = z + kLightZ * s;
          if (qz < kShoreZ)
            break;
          if (qy < TerrainAD(qx, qz)) {
            lit = 0;
            break;
          }
        }
      }
      depth_[k] = z;
      alt_[k] = py;
      sun_[k] = lit;
      up_[k] = ny;
      float grain = Fbmf(px * 0.4f, py * 0.25f, 2, 0);
      float gully = Ridged(px * 0.22f + z * 0.05f, py * 0.045f, 2);
      snow_[k] = Smoothf(0.36f, 0.52f, ny + 0.3f * (grain - 0.5f)) *
                 Smoothf(5, 11, py + 5 * grain) *
                 (1 - 0.75f * Smoothf(0.62f, 0.85f, gully));
    }
  }

  // 山脊线
  for (int k = kWidth; k < kShoreRow * kWidth; ++k)
    rim_[k] = (depth_[k] > 0 && depth_[k - kWidth] == 0 && sun_[k] > 0) ? 1 : 0;

  // ---------- 远岸树线 ----------
  for (int x = 0; x < kWidth; ++x)
    tree_top_[x] = kShoreRow - 0.6f - 1.2f * Fbmf(x * 0.06f, 2.3f, 2, 0);
  for (float tx = -2.0f; tx < kWidth + 2.0f;
       tx += 1.6f + Hashf(static_cast<int>((tx * 9.0f)), 7) * 2.2f) {
    float tip = kShoreRow - 2.6f - Hashf(static_cast<int>((tx * 3.0f)), 8) * 4 -
                1.5f * Smoothf(60, 0, tx);
    float slope = 1.1f + Hashf(static_cast<int>((tx * 5.0f)), 9) * 0.5f;
    int xa = std::max(0, static_cast<int>(std::floor(tx - 6.0f)));
    for (int x = xa; x < kWidth && static_cast<float>(x) < tx + 6.0f; ++x) {
      tree_top_[x] =
          std::min(tree_top_[x], tip + std::fabs(x + 0.5f - tx) * slope);
    }
  }

  // ---------- 湖面：每格反射射线落到上方图像的哪一行 ----------
  for (int r = kShoreRow; r < kHeight; ++r) {
    float v = ((kEyeRow - (r + 0.5f)) / 100.0f) * kFovTan;
    for (int x = 0; x < kWidth; ++x) {
      float u = ((x + 0.5f - 100.0f) / 100.0f) * kFovTan;
      float z = MarchAD(u, -v, -kCamHeight);
      float vs = -v - (z != 0 ? (2 * kCamHeight) / z : 0);
      float row = kEyeRow - (vs * 100.0f) / kFovTan - 0.5f;
      float mirror = 2.0f * kShoreRow - 1.0f - r;
      if (mirror >= tree_top_[x])
        row = mirror;
      src_[(r - kShoreRow) * kWidth + x] = row;
    }
  }

  // ---------- 近景松树与岸 ----------
  static const float kPines[6][3] = {
      {9, 5, 1.15f},    {20, 38, 0.85f},  {32, 66, 0.50f},
      {192, 10, 1.15f}, {181, 40, 0.75f}, {204, 26, 1.00f},
  };
  for (int r = 0; r < kHeight; ++r) {
    for (int x = 0; x < kWidth; ++x) {
      int k = r * kWidth + x;
      float y = r + 0.5f;
      float bankL = 88 + 14 * Smoothf(0, 52, static_cast<float>(x)) +
                    2 * Fbmf(x * 0.2f, 1, 2, 0);
      float bankR =
          90 +
          12 * Smoothf(static_cast<float>(kWidth),
                       static_cast<float>(kWidth) - 40, static_cast<float>(x)) +
          2 * Fbmf(x * 0.2f, 4, 2, 0);
      if (y > bankL || y > bankR) {
        fg_[k] = 1;
        fg_shade_[k] = 0.15f * Hashf(x, r);
      }
      for (int pi = 0; pi < 6; ++pi) {
        float px = kPines[pi][0], tip = kPines[pi][1], s = kPines[pi][2];
        float d = y - tip;
        if (d < 0)
          continue;
        float tier = 3.4f * s;
        float f = d / tier - std::floor(d / tier);
        float hw =
            (0.4f + d * 0.2f) * (0.5f + 0.5f * f) *
            (1 + 0.6f *
                     (Hashf(r, static_cast<int>(std::floor(px)) * 7) - 0.5f) *
                     Smoothf(0, 8, d));
        float dx = x + 0.5f - px;
        if (std::fabs(dx) <= hw) {
          fg_[k] = 2;
          bool edge = hw - std::fabs(dx) < 1;
          bool sunward = px < kSunX ? dx > 0 : dx < 0;
          fg_shade_[k] =
              edge ? (sunward ? 1.0f : 0.5f) : 0.3f * Hashf(x * 3, r * 5);
          fg_rim_[k] =
              edge ? Smoothf(70, 44, y) * (0.75f + 0.25f * Hashf(x, r * 7)) : 0;
        }
      }
    }
  }

  // ---------- 雾层 ----------
  for (int r = 0; r < kMistRows; ++r) {
    for (int x = 0; x < kMistW; ++x) {
      float y = r + kMistTop;
      float q =
          Fbmf(x * 0.0125f, y * 0.1f, 2, static_cast<int>((kMistW * 0.0125f)));
      mist_[r * kMistW + x] = Fbmf(x * 0.025f + q * 1.4f, y * 0.22f + q, 4,
                                   static_cast<int>((kMistW * 0.025f)));
    }
  }

  // ---------- 高云 ----------
  for (int r = 0; r < kCloudRows; ++r) {
    for (int x = 0; x < kCloudW; ++x) {
      float y = r + 0.5f;
      float q = Fbmf(x * 0.0125f, y * 0.12f, 2, 8);
      float c = Fbmf(x * 0.025f + q * 2, y * 0.2f + q * 0.8f, 4, 16);
      cloud_[r * kCloudW + x] =
          Smoothf(0.52f, 0.7f, c - 0.06f * std::fabs(y - 18) / 10) *
          Smoothf(5, 13, y) * Smoothf(33, 24, y);
    }
  }

  // ---------- 全域低频噪声 ----------
  for (int k = 0; k < N; ++k)
    grain_[k] =
        Fbmf((k % kWidth) * 0.03f,
             std::floor(static_cast<float>((k / kWidth))) * 0.06f, 3, 0);

  // ---------- 天空颜色 + 太阳辉光 ----------
  for (int r = 0; r < kShoreRow; ++r) {
    for (int x = 0; x < kWidth; ++x) {
      int k = r * kWidth + x;
      float y = r + 0.5f;
      float v = Clampf(y / 52.0f);
      float east = Smoothf(20, 190, static_cast<float>(x));
      float dx = x + 0.5f - kSunX, dy = (y - kSunY) * 2.2f;
      float ds = std::sqrt(dx * dx + dy * dy);
      float low = std::pow(v, 1.9f);
      float veil = (grain_[k] - 0.5f) * 0.1f * (1 - v);
      float cr = 0.03f + veil + low * (0.56f + 0.20f * east);
      float cg = 0.04f + veil + low * (0.48f + 0.02f * east);
      float cb = 0.13f + veil * 1.6f + low * (0.62f - 0.12f * east);
      sky_r_[k] = cr;
      sky_g_[k] = cg;
      sky_b_[k] = cb;
      glow_a_[k] = std::exp(-ds / 6) * 0.65f + std::exp(-ds / 15) * 0.2f +
                   std::exp(-ds / 50) * 0.1f;
    }
  }
}

// ---------- 每帧 -----------------------------------------------------------
void AlpineDawn::Update(double t) {
  const float tf = static_cast<float>(t);

  const float warm = 0.75f - 0.5f * std::exp(-tf / 60.0f);  // 玫瑰→金
  const float line = 6.0f + 4.0f * std::exp(-tf / 70.0f);   // 日照线向下爬
  const float drift = tf * 1.1f;
  const float pulse =
      1.0f + 0.06f * std::sin((tf / 8.0f) * static_cast<float>(M_PI) * 2.0f);

  for (int x = 0; x < kWidth; ++x)
    wisp_[x] = Noisef((x + drift * 0.6f) * 0.06f, 3.7f, 0);

  // 暖光：玫瑰 → 金
  const float lr = 1.0f;
  const float lg = Mixf(0.6f, 0.8f, warm);
  const float lb = Mixf(0.55f, 0.4f, warm);

  // ---------- 第一遍：天空 + 山 ----------
  for (int r = 0; r < kShoreRow; ++r) {
    const float y = r + 0.5f;
    for (int x = 0; x < kWidth; ++x) {
      const int k = r * kWidth + x;
      const float gl = glow_a_[k] * pulse;
      const float gg = 0.62f + 0.28f * Smoothf(0.15f, 0.7f, gl);
      float cr = sky_r_[k] + gl;
      float cg = sky_g_[k] + gl * gg;
      float cb = sky_b_[k] + gl * (gg - 0.22f);
      float fl = 0.21f;
      const float z = depth_[k];
      if (z != 0) {
        const float sn = snow_[k];
        const float lit = sun_[k] * Smoothf(line, line + 7, alt_[k]);
        const float amb = (0.55f + 0.45f * up_[k]) *
                          (0.55f + 0.50f * Smoothf(4, 34, alt_[k]));
        const float sl = Smoothf(0.08f, 0.24f, lit);
        const float gold = Clampf(0.6f * Smoothf(0.25f, 0.8f, lit) +
                                  0.5f * Smoothf(14, 36, alt_[k]));
        const float br = 0.78f + 0.3f * lit;
        const float sr = Mixf(0.13f * amb, lr * br, sl);
        const float sg =
            Mixf(0.17f * amb, Mixf(lg - 0.12f, lg + 0.14f, gold) * br, sl);
        const float sb =
            Mixf(0.36f * amb, Mixf(lb + 0.02f, lb + 0.12f, gold) * br, sl);
        const float rr = Mixf(0.06f, 0.40f, sl);
        const float rg = Mixf(0.07f, 0.20f, sl);
        const float rb = Mixf(0.14f, 0.20f, sl);
        cr = Mixf(rr, sr, sn);
        cg = Mixf(rg, sg, sn);
        cb = Mixf(rb, sb, sn);
        // 山脚森林
        const float wood = Smoothf(9, 4, alt_[k]) * Smoothf(110, 75, z);
        cr = Mixf(cr, 0.07f, wood);
        cg = Mixf(cg, 0.09f, wood);
        cb = Mixf(cb, 0.18f, wood);
        // 距离雾
        const float fog = Smoothf(16, 3, alt_[k]) * Smoothf(70, 150, z) * 0.6f;
        const float haze = std::max(
            fog, Clampf(1 - std::exp(-(z - kShoreZ) / 260.0f)) * 0.45f);
        cr = Mixf(cr, sky_r_[k] + gl, haze);
        cg = Mixf(cg, sky_g_[k] + gl * gg, haze);
        cb = Mixf(cb, sky_b_[k] + gl * (gg - 0.22f), haze);
        // 山脊线
        if (rim_[k] && sl > 0.3f) {
          const float a = rim_[k] * sl;
          cr = Mixf(cr, 1.0f, a);
          cg = Mixf(cg, Mixf(0.89f, 0.95f, warm), a);
          cb = Mixf(cb, Mixf(0.76f, 0.88f, warm), a);
        }
        fl = Mixf(Mixf(0.42f, 0.3f, wood), 0.04f, sl);
      } else {
        // 西边残留的星
        if (y < 34 && Hashf(x, r * 3 + 11) > 0.985f) {
          const float tw =
              0.6f + 0.4f * std::sin(tf * (1.5f + Hashf(x, r) * 3) +
                                     Hashf(r, x) * 6.28f);
          const float s = tw * Smoothf(150, 40, static_cast<float>(x)) *
                          Smoothf(34, 6, y) * 0.75f;
          cr = std::max(cr, s * 0.9f);
          cg = std::max(cg, s * 0.92f);
          cb = std::max(cb, s);
        }
        // 高云
        if (r < kCloudRows) {
          float sx = x + tf * 0.8f;
          float ix = std::floor(sx), fx = sx - ix;
          int ii = static_cast<int>(ix);
          int c0i = r * kCloudW + ((ii % kCloudW) + kCloudW) % kCloudW;
          int c1i = r * kCloudW + (((ii + 1) % kCloudW) + kCloudW) % kCloudW;
          float c = (cloud_[c0i] + (cloud_[c1i] - cloud_[c0i]) * fx) *
                    (0.35f + 0.65f * Smoothf(40, 150, static_cast<float>(x)));
          if (c > 0.01f) {
            float dx = x + 0.5f - kSunX;
            float dy = (y - kSunY) * 1.6f;
            float g = std::exp(-std::sqrt(dx * dx + dy * dy) / 55.0f);
            float b = Clampf(0.25f + 0.9f * g);
            float kr = Mixf(0.32f, 1.0f, b);
            float kg = Mixf(0.24f, Mixf(0.62f, 0.74f, warm), b);
            float kb = Mixf(0.40f, 0.50f, b);
            cr = Mixf(cr, kr, c * 0.75f);
            cg = Mixf(cg, kg, c * 0.75f);
            cb = Mixf(cb, kb, c * 0.75f);
          }
        }
        // 太阳
        float dx = x + 0.5f - kSunX, dy = y - kSunY;
        float ds = std::sqrt(dx * dx + dy * dy);
        if (ds < 4.5f) {
          float a = Smoothf(4.5f, 3.3f, ds);
          cr = Mixf(cr, 1.0f, a);
          cg = Mixf(cg, 0.96f, a);
          cb = Mixf(cb, 0.86f, a);
        }
      }
      // 山谷里蓄着的雾
      const float e =
          Smoothf(20, 170, static_cast<float>(x)) * (0.6f + 0.4f * warm);
      const float near = std::exp(-std::fabs(x + 0.5f - kSunX) / 22.0f) * 0.3f;
      const float mr = Mixf(0.48f, 0.9f, e) + near;
      const float mg = Mixf(0.46f, 0.7f, e) + near * 0.75f;
      const float mb = Mixf(0.70f, 0.7f, e) + near * 0.50f;

      if (r >= kMistTop) {
        int mi =
            static_cast<int>((std::floor(x + drift) + kMistW * 1000)) % kMistW;
        float m = mist_[(r - kMistTop) * kMistW + mi];
        float edge = 5.0f * (m - 0.5f) + 4.0f * (wisp_[x] - 0.5f);
        float band = Smoothf(kMistTop + 14.0f, kShoreRow - 3.0f, y + edge);
        float a = (0.3f + 0.7f * Smoothf(0.32f, 0.64f, m)) * band * 0.6f;
        cr = Mixf(cr, mr, a);
        cg = Mixf(cg, mg, a);
        cb = Mixf(cb, mb, a);
        if (a > 0.05f)
          fl = std::max(fl, 0.2f);
      }
      if (y >= tree_top_[x]) {
        float s = 0.4f + 0.6f * Hashf(x * 7, r * 3);
        cr = 0.04f + 0.03f * s;
        cg = 0.06f + 0.04f * s;
        cb = 0.10f + 0.05f * s;
        fl = 0.0f;
        int mi =
            static_cast<int>((std::floor(x * 0.7f + drift * 1.9f + 211.0f) +
                              kMistW * 1000)) %
            kMistW;
        float m = mist_[(r - kMistTop) * kMistW + mi];
        float a =
            Smoothf(0.45f, 0.72f, m) *
            Smoothf(tree_top_[x] + 1.0f, static_cast<float>(kShoreRow), y) *
            0.6f;
        cr = Mixf(cr, mr, a);
        cg = Mixf(cg, mg, a);
        cb = Mixf(cb, mb, a);
      }

      a_r_[k] = cr;
      a_g_[k] = cg;
      a_b_[k] = cb;
      f_r_[k] = cr;
      f_g_[k] = cg;
      f_b_[k] = cb;
      flo_[k] = fl;
    }
  }

  // ---------- 湖面 ----------
  for (int r = kShoreRow; r < kHeight; ++r) {
    const float y = r + 0.5f;
    const float d = (y - kShoreRow) / static_cast<float>(kLakeRow);
    for (int x = 0; x < kWidth; ++x) {
      const int k = r * kWidth + x;
      float w1 = Noisef(x * 0.045f + tf * 0.06f, y * 0.5f - tf * 0.35f, 0);
      float w2 = Noisef(x * 0.12f - tf * 0.10f, y * 1.1f - tf * 0.70f, 0);
      float sway = (w1 - 0.5f) * (0.4f + 1.4f * d) + (w2 - 0.5f) * 0.5f;
      int sx = std::max(
          0, std::min(kWidth - 1, static_cast<int>(std::round(x + sway))));
      int sr = std::max(
          0, std::min(kShoreRow - 1, static_cast<int>(std::round(
                                         src_[(r - kShoreRow) * kWidth + x] +
                                         (w2 - 0.5f) * 0.6f * d))));
      int sk = sr * kWidth + sx;
      float refl = 0.68f - 0.32f * d;
      float w3 = Noisef(x * 0.03f + tf * 0.04f, y * 1.9f - tf * 0.45f, 0);
      float lift = 1.0f + (w3 - 0.5f) * (0.4f + 0.5f * d);
      float ar = a_r_[sk], ag = a_g_[sk], ab = a_b_[sk];
      float grey = (ar + ag + ab) / 3.0f;
      float deep = (grain_[k] - 0.5f) * 0.1f * d;
      float cr = 0.020f + deep + Mixf(grey, ar, 0.75f) * refl * lift;
      float cg = 0.035f + deep + Mixf(grey, ag, 0.75f) * refl * lift;
      float cb = 0.070f + deep * 1.6f + Mixf(grey, ab, 0.75f) * refl * lift;
      float roadW = 1.5f + (y - kShoreRow) * 0.45f;
      float road = std::exp(-std::pow((x + 0.5f - kSunX) / roadW, 2.f));
      float glint = Smoothf(0.55f, 0.85f, w2) * road * (0.5f + 0.5f * warm);
      cr += glint;
      cg += glint * 0.8f;
      cb += glint * 0.6f;
      f_r_[k] = cr;
      f_g_[k] = cg;
      f_b_[k] = cb;
      flo_[k] = 0.26f;
      fade_[k] = Smoothf(kHeight + 2.0f, kHeight - 22.0f, y);
      if (r == kShoreRow) {
        f_r_[k] = 0.06f;
        f_g_[k] = 0.12f;
        f_b_[k] = 0.14f;
        flo_[k] = 0.0f;
      }
    }
  }

  // ---------- 前景松树与岸 ----------
  const int N = kWidth * kHeight;
  for (int k = 0; k < N; ++k) {
    if (!fg_[k])
      continue;
    float s = fg_shade_[k];
    f_r_[k] = 0.02f + 0.05f * s;
    f_g_[k] = 0.04f + 0.06f * s;
    f_b_[k] = 0.05f + 0.06f * s;
    flo_[k] = 0.0f;
    float e = fg_rim_[k];
    if (e > 0 && s == 1.0f) {
      f_r_[k] = Mixf(f_r_[k], 0.62f, e);
      f_g_[k] = Mixf(f_g_[k], 0.40f, e);
      f_b_[k] = Mixf(f_b_[k], 0.38f, e);
      flo_[k] = 0.12f * e;
    } else if (e > 0 && s == 0.5f) {
      f_r_[k] = Mixf(f_r_[k], 0.20f, e);
      f_g_[k] = Mixf(f_g_[k], 0.22f, e);
      f_b_[k] = Mixf(f_b_[k], 0.36f, e);
      flo_[k] = 0.22f * e;
    }
    fade_[k] = 1.0f;
  }

  // ---------- 量化 ----------
  for (int r = 0; r < kHeight; ++r) {
    for (int x = 0; x < kWidth; ++x) {
      const int k = r * kWidth + x;
      const float cr = f_r_[k], cg = f_g_[k], cbl = f_b_[k], fl = flo_[k];
      const float peak = std::max({cr, cg, cbl, 1e-4f});
      const float level =
          Clampf(fl + (1 - fl) * std::pow(peak, 1.1f)) * fade_[k];
      const int step = std::max(
          0,
          std::min(3, static_cast<int>(std::round(
                          level * 3 + kHalftoneBayer[(r & 3) * 4 + (x & 3)]))));
      step_[k] = static_cast<uint8_t>(step);
      if (step) {
        float want = std::min(1.0f, (level + 0.06f) / kHalftoneCover[step]);
        float s = ((0.3f + 0.7f * want) *
                   Mixf(0.5f, 1.0f, Smoothf(0.08f, 0.5f, peak))) /
                  peak;
        color_[k] = static_cast<uint8_t>(
            palette_.Nearest(Clampf(cr * s), Clampf(cg * s), Clampf(cbl * s)));
      } else {
        color_[k] = 0;
      }
    }
  }
}