#include "desert_night.h"

#include <algorithm>
#include <cmath>

#include <QColor>

// ============================== 常量 =======================================
static constexpr int kHorizon = 64;              // 眼平线行
static constexpr float kK = 0.62f;               // 视场半角的切线
static constexpr float kCam = 6.0f;              // 相机高度
static constexpr float kHmax = 16.0f;            // 最高沙丘
static constexpr int kTownX = 30, kTownY = 66;   // 远处小镇
static constexpr int kCoreX = 152, kCoreY = 38;  // 银河核心
static constexpr float kArcX = 50.0f, kArcY = 209.8f, kArcR = 199.8f;
static constexpr float kArcS0 = -1.035f, kArcS1 = 0.8f;
static constexpr int kAcacia = 150;

// 归一化光照方向
static const float kLD[3] = {-0.8031f, 0.3212f, 0.5019f};  // 小镇暖光，前左低处
static const float kGD[3] = {0.6837f, 0.5697f, 0.4558f};   // 银河冷填充，右上

static const char* const kPaletteHex[DesertNight::kPaletteN] = {
    "#0a1024", "#101830", "#16213f", "#1e2b50", "#283864", "#34477a", "#45598f",
    "#5a6fa6", "#7488bd", "#93a5d2", "#b6c3e4", "#d8e0f2", "#f4f6fb", "#fff3dc",
    "#ffe2b4", "#f5c98e", "#e2a86e", "#c4864f", "#9c6440", "#74492f", "#2a2230",
    "#3d2f3a", "#56404a", "#735358", "#946a62", "#b6836c", "#4a3b48", "#6b5562",
    "#8a6f7a", "#a88590", "#c9a3a3", "#e2bfb4", "#2a2448", "#3d3466", "#57498a",
    "#7a68a8", "#ffd8a8", "#cfe0ff", "#a9c4ff", "#ff9a52", "#e07a3e", "#0d0f1a",
    "#151827",
};

enum MatD { kSkyD = 0, kSandD = 1, kTreeD = 2 };

// ============================== 沙丘场 =====================================
// 波状相位：波谷 0，迎风面缓升到 0.72 处的脊，背风面陡降
static inline float PhaseD(float x, float z) {
  float p =
      x * 0.9f + z * 0.42f + 26.f * Fbmf(x * 0.014f, z * 0.014f, 3, 0, 31.7f);
  float q = p / 19.f;
  return q - std::floor(q);
}

// 孤树所在的大沙丘：脊线随 z 蛇形，两侧一缓一陡
static inline float CrestXD(float z) {
  float k = Clampf((z - 22.f) / 52.f);
  return -4.f + 26.f * k * k * (3.f - 2.f * k) +
         5.f * std::sin((z - 22.f) * 0.09f);
}
static inline float RidgeD(float x, float z) {
  if (z < 8.f)
    return 0.f;
  float hc = z <= 74.f
                 ? 0.5f + 13.f * std::pow(Clampf((z - 12.f) / 62.f), 1.25f)
                 : 13.5f * (1.f - (z - 74.f) / 9.f);
  float dx = x - CrestXD(z);
  return hc - (dx < 0.f ? -dx * 0.36f : dx * 0.8f);
}

static inline float DunesD(float x, float z) {
  float u = PhaseD(x, z);
  float prof = u < 0.72f ? std::pow(u / 0.72f, 1.5f)
                         : std::pow((1.f - u) / 0.28f, 0.75f);
  float rg = RidgeD(x, z);
  float amp = (1.f + 4.5f * Fbmf(x * 0.008f + 5.f, z * 0.008f, 2, 0)) *
              (1.f - 0.8f * Smoothf(-3.f, 3.f, rg));
  return std::max(prof * amp, rg) + 0.3f * Fbmf(x * 0.05f, z * 0.05f, 2, 0);
}

// 沿射线（斜率 v，横比 u）步进到沙丘，返回距离（0 = 天空）
static float MarchD(float u, float v) {
  float z = 3.f, prev = z;
  for (int i = 0; i < 300 && z < 600.f; ++i) {
    float y = kCam + v * z;
    if (v > 0.f && y > kHmax)
      return 0.f;
    float gap = y - DunesD(u * z, z);
    if (gap < 0.f) {
      float a = prev, b = z;
      for (int j = 0; j < 7; ++j) {
        float m = (a + b) * 0.5f;
        if (kCam + v * m - DunesD(u * m, m) < 0.f)
          b = m;
        else
          a = m;
      }
      return b;
    }
    prev = z;
    z += std::max(0.25f, gap * 0.5f) + z * 0.004f;
  }
  return 0.f;
}

// ============================== 流星参数 ===================================
// 每颗流星占一个 7 秒槽位；流星 0 是开局就已经在落的那一颗
static void MeteorParams(int n, float& start, float& x0, float& y0, float& dx,
                         float& dy, float& speed) {
  start = (n == 0)
              ? -0.35f
              : 7.f * static_cast<float>(n - 1) + 2.5f + Hashf(n, 71) * 2.5f;
  x0 = (n == 0) ? 199.f : 30.f + Hashf(n, 72) * 140.f;
  y0 = (n == 0) ? 4.f : 4.f + Hashf(n, 73) * 18.f;
  float a = (n == 0)
                ? 2.65f
                : (x0 > 100.f ? 2.6f : 0.55f) + (Hashf(n, 75) - 0.5f) * 0.4f;
  dx = std::cos(a);
  dy = std::sin(a);
  speed = 52.f + Hashf(n, 76) * 30.f;
}

// ============================== 公开接口 ===================================
const char* const* DesertNight::PaletteHex() { return kPaletteHex; }

DesertNight::DesertNight() { Init(); }

// ============================== 静态构建 ===================================
void DesertNight::Init() {
  palette_.Init(kPaletteHex, kPaletteN);

  const int N = kWidth * kHeight;
  mat_.assign(N, kSkyD);
  depth_.assign(N, 0.f);
  sand_r_.assign(N, 0.f);
  sand_g_.assign(N, 0.f);
  sand_b_.assign(N, 0.f);
  crest_.assign(N, 0.f);
  bark_.assign(N, 0.f);
  top_.fill(kHeight);
  sky_r_.assign(N, 0.f);
  sky_g_.assign(N, 0.f);
  sky_b_.assign(N, 0.f);
  air_.assign(N, 0.f);
  band_.assign(N, 0.f);
  dith_.assign(N, 0.f);
  floor0_.assign(N, 0.f);
  fr_.assign(N, 0.f);
  fg_.assign(N, 0.f);
  fb_.assign(N, 0.f);
  floor_.assign(N, 0.f);
  step_.assign(N, 0);
  color_.assign(N, 0);

  // ---------- 沙丘：一次性 raymarch ----------
  for (int r = 0; r < kHeight; ++r) {
    float v = ((kHorizon - (r + 0.5f)) / 100.f) * kK;
    for (int x = 0; x < kWidth; ++x) {
      float u = ((x + 0.5f - 100.f) / 100.f) * kK;
      float z = MarchD(u, v);
      if (z == 0.f)
        continue;
      int k = r * kWidth + x;
      mat_[k] = kSandD;
      depth_[k] = z;
      if (r < top_[x])
        top_[x] = static_cast<int16_t>(r);

      float px = u * z, py = kCam + v * z;
      const float e = 0.2f;
      float hx = (DunesD(px + e, z) - DunesD(px - e, z)) / (2.f * e);
      float hz = (DunesD(px, z + e) - DunesD(px, z - e)) / (2.f * e);
      float nl = std::sqrt(hx * hx + 1.f + hz * hz);
      float nx = -hx / nl, ny = 1.f / nl, nz = -hz / nl;

      float lit = std::max(0.f, nx * kLD[0] + ny * kLD[1] + nz * kLD[2]);
      if (lit > 0.f) {
        // 软阴影：射向小镇的光线离沙面最近有多远
        float sh = 1.f;
        for (float s = 0.6f; s < 90.f; s += 0.5f + s * 0.05f) {
          float qx = px + kLD[0] * s;
          float qy = py + kLD[1] * s + 0.05f;
          float qz = z + kLD[2] * s;
          if (qy > kHmax)
            break;
          sh = std::min(sh, (qy - DunesD(qx, qz)) / (0.08f * s));
          if (sh <= 0.f)
            break;
        }
        lit *= Clampf(sh);
      }
      lit = std::min(1.f, std::pow(lit, 1.4f) * 1.45f);

      // 脊上最亮，每个面往谷里暗下去
      float ph = PhaseD(px, z);
      bool onRidge = RidgeD(px, z) >= DunesD(px, z) - 0.35f;
      float dxc = px - CrestXD(z);
      float fall = onRidge ? 0.5f + 0.5f * std::exp(-std::abs(dxc) / 7.f)
                           : 0.55f + 0.45f * Smoothf(0.2f, 0.72f, ph);
      float near = 0.72f + 0.28f * Smoothf(5.f, 25.f, z);
      float reach = (0.55f + 0.45f * std::exp(-std::abs(x - kTownX) / 90.f)) *
                    fall * near * Smoothf(300.f, 120.f, z);
      float fill =
          0.045f * std::max(0.f, nx * kGD[0] + ny * kGD[1] + nz * kGD[2]);
      float ripple =
          (Fbmf(px * 0.6f + z * 0.2f, z * 0.6f, 2, 0) - 0.5f) * 0.05f;
      float amb = (0.02f + 0.03f * ny + ripple) * near;

      float cr = amb * 0.7f + fill * 0.45f + lit * 1.0f * reach;
      float cg = amb * 0.62f + fill * 0.65f + lit * 0.63f * reach;
      float cb = amb * 1.35f + fill * 1.2f + lit * 0.53f * reach;

      // 大沙丘背光侧的脊线被星光勾亮
      if (onRidge && z < 76.f && dxc > 0.f) {
        float cellW = z * kK * 0.01f;
        float rim =
            Smoothf(2.2f * cellW, 0.6f * cellW, dxc) * Smoothf(10.f, 20.f, z);
        cr += 0.1f * rim;
        cg += 0.14f * rim;
        cb += 0.24f * rim;
      }
      // 距离把光稀释进地平线的霾里
      float haze = Clampf(1.f - std::exp(-z / 260.f)) *
                   Mixf(0.7f, 0.92f, Smoothf(120.f, 300.f, z));
      float hg = std::exp(-std::abs(x - kTownX) / 40.f) * 0.18f;
      cr = Mixf(cr, 0.07f + hg, haze);
      cg = Mixf(cg, 0.085f + hg * 0.6f, haze);
      cb = Mixf(cb, 0.15f + hg * 0.3f, haze);

      sand_r_[k] = cr;
      sand_g_[k] = cg;
      sand_b_[k] = cb;

      float big = (RidgeD(px, z) > 0.5f && z < 76.f)
                      ? Smoothf(0.9f, 0.f, std::abs(dxc))
                      : 0.f;
      crest_[k] = std::max(Smoothf(0.07f, 0.f, std::abs(ph - 0.72f)) *
                               Smoothf(260.f, 30.f, z),
                           big) *
                  (0.3f + 0.7f * Smoothf(0.f, 0.2f, lit));
    }
  }

  // ---------- 金合欢树：银河核心下方沙脊上的孤树 ----------
  {
    const int base = top_[kAcacia];
    const int canopyTop = base - 16;
    const float span = 17.f;

    struct Limb {
      float ax, ay, bx, by, w;
    };
    const float forkX = kAcacia + 0.3f, forkY = static_cast<float>(base) - 5.f;
    const Limb limbs[] = {
        {static_cast<float>(kAcacia), static_cast<float>(base) + 1.f, forkX,
         forkY, 1.3f},
        {forkX, forkY, static_cast<float>(kAcacia - 9),
         static_cast<float>(canopyTop + 3), 0.8f},
        {forkX, forkY, static_cast<float>(kAcacia) + 1.5f,
         static_cast<float>(canopyTop + 2), 0.75f},
        {forkX, forkY, static_cast<float>(kAcacia + 10),
         static_cast<float>(canopyTop) + 3.2f, 0.8f},
        {forkX, forkY, static_cast<float>(kAcacia - 4),
         static_cast<float>(canopyTop) + 2.6f, 0.55f},
        {static_cast<float>(kAcacia - 4), static_cast<float>(base - 9),
         static_cast<float>(kAcacia - 14), static_cast<float>(canopyTop) + 3.5f,
         0.55f},
        {static_cast<float>(kAcacia + 3), static_cast<float>(base - 8),
         static_cast<float>(kAcacia + 15), static_cast<float>(canopyTop) + 3.6f,
         0.5f},
    };

    for (int r = std::max(0, canopyTop - 3); r <= base + 1 && r < kHeight;
         ++r) {
      for (int x = kAcacia - 26; x <= kAcacia + 26; ++x) {
        if (x < 0 || x >= kWidth)
          continue;
        const float px = x + 0.5f, py = r + 0.5f;
        const float q = (px - kAcacia) / span;

        bool on = false;
        if (std::abs(q) <= 1.08f) {
          float topY = canopyTop + 0.4f + 2.6f * q * q +
                       1.3f * (Noisef(px * 0.3f, 7.1f, 0) - 0.5f);
          float botY = canopyTop + 3.8f + 1.4f * (1.f - q * q) -
                       1.4f * Smoothf(0.8f, 1.08f, std::abs(q)) +
                       1.2f * (Noisef(px * 0.45f, 3.3f, 0) - 0.5f) +
                       (Hashf(x, 51) < 0.2f ? 0.9f : 0.f);
          if (py > topY && py < botY)
            on = true;
        }
        for (const auto& lb : limbs) {
          float lx = lb.bx - lb.ax, ly = lb.by - lb.ay;
          float t = Clampf(((px - lb.ax) * lx + (py - lb.ay) * ly) /
                           (lx * lx + ly * ly));
          float dx = px - (lb.ax + lx * t);
          float dy = py - (lb.ay + ly * t);
          float ww = lb.w * (1.f - 0.4f * t);
          if (dx * dx + dy * dy < ww * ww)
            on = true;
        }
        if (on)
          mat_[r * kWidth + x] = kTreeD;
      }
    }

    // 树冠上缘，被银河核心的凸起勾亮
    for (int r = 1; r < kHeight; ++r) {
      for (int x = 0; x < kWidth; ++x) {
        int k = r * kWidth + x;
        if (mat_[k] != kTreeD || mat_[k - kWidth] != kSkyD)
          continue;
        float dx = x + 0.5f - kCoreX;
        float dy = (r - kCoreY) * 1.3f;
        bark_[k] =
            0.25f + 0.75f * std::exp(-std::sqrt(dx * dx + dy * dy) / 18.f);
      }
    }
  }

  // ---------- 小镇灯光：远处地平线上几颗针尖 ----------
  lamps_.clear();
  for (int x = kTownX - 9;
       x <= kTownX + 9 && static_cast<int>(lamps_.size()) < 5; ++x) {
    int r = top_[x];
    if (r >= kHeight || depth_[r * kWidth + x] < 140.f || Hashf(x, 91) > 0.45f)
      continue;
    bool tooClose = false;
    for (const auto& l : lamps_)
      if (std::abs((l.k % kWidth) - x) < 2) {
        tooClose = true;
        break;
      }
    if (tooClose)
      continue;
    Lamp lp;
    lp.k = r * kWidth + x;
    lp.phase = Hashf(x, 92) * 6.28f;
    lp.rate = 0.7f + Hashf(x, 93) * 1.4f;
    lamps_.push_back(lp);
  }

  // ---------- 天空：渐变、小镇辉光、气辉、银河 ----------
  for (int r = 0; r < kHeight; ++r) {
    const float y = r + 0.5f;
    const float v = Clampf(y / kHorizon);
    for (int x = 0; x < kWidth; ++x) {
      const int k = r * kWidth + x;

      // 饱和的深蓝，抖动出的小点才读得出颜色
      float cr = 0.02f + 0.03f * v * v;
      float cg = 0.035f + 0.045f * v * v;
      float cb = 0.1f + 0.08f * v * v;

      // 小镇：地平线下方升起的琥珀色圆顶，暖光取代深蓝而非灰化它
      const float tx = x + 0.5f - kTownX;
      const float ty = (y - kTownY) * 1.9f;
      const float td = std::sqrt(tx * tx + ty * ty);
      const float glow =
          std::exp(-td / 16.f) * 0.4f + std::exp(-td / 60.f) * 0.22f;
      const float cool = 1.f - 0.85f * Clampf(std::exp(-td / 22.f) * 1.5f);
      cr = cr * cool + glow;
      cg = cg * cool + glow * 0.5f;
      cb = cb * cool + glow * 0.16f;

      // 气辉：低空一层微弱蓝光，避开小镇，每帧缓慢漂移
      const float along =
          0.72f + 0.5f * Fbmf(x * 0.025f + 3.f, y * 0.04f, 2, 0);
      air_[k] = std::pow(Smoothf(0.3f, 0.94f, v), 3.f) *
                (1.f - 0.3f * Smoothf(0.92f, 1.02f, v)) * 0.32f * cool * along;

      // 银河：从核心沿弧线向西升起的带
      const float ax = x + 0.5f - kArcX;
      const float ay = y - kArcY;
      const float d = std::sqrt(ax * ax + ay * ay) - kArcR;
      const float sRaw = (kArcS0 - std::atan2(ay, ax)) / kArcS1;
      const float s = Clampf(sRaw);
      const float end =
          sRaw < 0.f ? std::exp(-((sRaw / 0.05f) * (sRaw / 0.05f))) : 1.f;
      const float laneEnd =
          sRaw < 0.f ? std::exp(-((sRaw / 0.11f) * (sRaw / 0.11f))) : 1.f;
      const float w = 9.f + 8.f * std::pow(1.f - s, 1.4f);
      const float lane0 =
          (Noisef(s * 9.f, 3.3f, 0) - 0.5f) * w * 0.5f * Smoothf(0.f, 0.25f, s);
      float b = std::exp(-((d / w) * (d / w)));
      const float clump = Fbmf(x * 0.09f, y * 0.09f, 4, 0);
      b *= 0.35f + 1.1f * Smoothf(0.3f, 0.75f, clump);
      b *= (0.55f + 0.55f * (1.f - s)) * end;

      // 核心的凸起
      const float cx = x + 0.5f - kCoreX;
      const float cy = (y - kCoreY) * 1.3f;
      const float cd = std::sqrt(cx * cx + cy * cy);
      const float core =
          std::exp(-cd / 6.f) * 0.5f + std::exp(-cd / 22.f) * 0.25f;
      b += core;

      // 暗尘带：穿过凸起的一道清晰裂口，加上它的丝状结构
      const float nearCore = std::exp(-cd / 16.f);
      const float lw = w * 0.12f;
      const float dl = (d - lane0) / lw;
      const float lane =
          std::exp(-(dl * dl)) *
          Mixf((0.55f + 0.6f * Fbmf(x * 0.12f, y * 0.12f, 3, 0)) *
                   (0.55f + 0.45f * (1.f - s)),
               1.f, nearCore) *
          laneEnd;
      const float fil =
          Smoothf(0.58f, 0.78f, Fbmf(x * 0.16f + 9.f, y * 0.16f, 3, 0)) * 0.6f;
      const float dfil = d / (w * 1.3f);
      b *= Clampf(1.f - Mixf(0.92f, 0.97f, nearCore) * lane) *
           (1.f - fil * std::exp(-(dfil * dfil)));

      const float dbnd = d / (w * 1.2f);
      band_[k] = std::exp(-(dbnd * dbnd)) * end + core;

      // 核心偏暖，外臂偏冷
      const float warm =
          Clampf(std::exp(-cd / 26.f) * 1.2f + (1.f - s) * 0.25f);
      cr += b * Mixf(0.62f, 1.0f, warm) * 0.75f;
      cg += b * Mixf(0.68f, 0.9f, warm) * 0.75f;
      cb += b * Mixf(0.95f, 0.74f, warm) * 0.75f;

      sky_r_[k] = cr;
      sky_g_[k] = cg;
      sky_b_[k] = cb;
    }
  }

  // ---------- 每格抖动与 floor：开阔天空随机，银河带半有序 ----------
  for (int r = 0; r < kHeight; ++r) {
    for (int x = 0; x < kWidth; ++x) {
      const int k = r * kWidth + x;
      const float bay = kHalftoneBayer[(r & 3) * 4 + (x & 3)];
      if (mat_[k] == kSkyD) {
        const float inb = Clampf(band_[k]);
        const float soft = std::max(inb, Clampf(air_[k] / 0.2f));
        const float rnd =
            (Hashf(x * 3 + 1, r * 5 + 2) - 0.5f) * Mixf(0.6f, 0.94f, soft);
        dith_[k] = Mixf(rnd, bay, 0.5f * soft);
        floor0_[k] = 0.1f * inb;
      } else {
        dith_[k] = bay;
        floor0_[k] = (mat_[k] == kSandD) ? 0.02f : 0.f;
      }
    }
  }

  // ---------- 星星：银河带里密，全天稀疏几颗亮的 ----------
  stars_.clear();
  for (int r = 0; r < kHorizon + 2; ++r) {
    for (int x = 0; x < kWidth; ++x) {
      const int k = r * kWidth + x;
      if (mat_[k] != kSkyD)
        continue;
      const float p = 0.015f + 0.12f * Clampf(band_[k]);
      if (Hashf(x * 7 + 3, r * 13 + 1) > p)
        continue;
      const float m = std::pow(Hashf(x * 5 + 1, r * 3 + 7), 12.f);
      const float bright = (0.2f + 0.95f * m) *
                           Smoothf(kHorizon + 1.f, kHorizon - 6.f, r + 0.5f);
      const float c = Hashf(x * 11, r * 17 + 5);
      Star st;
      st.k = k;
      st.bright = bright;
      if (c < 0.2f) {
        st.tr = 0.78f;
        st.tg = 0.86f;
        st.tb = 1.0f;
      } else if (c < 0.9f) {
        st.tr = 1.f;
        st.tg = 1.f;
        st.tb = 1.f;
      } else {
        st.tr = 1.f;
        st.tg = 0.82f;
        st.tb = 0.6f;
      }
      st.rate = 1.5f + Hashf(x, r * 9) * 4.f;
      st.phase = Hashf(r, x * 3) * 6.28f;
      stars_.push_back(st);
    }
  }
}

// ---------- 每帧 -----------------------------------------------------------
void DesertNight::Update(double t) {
  const float tf = static_cast<float>(t);
  const int N = kWidth * kHeight;

  // 天空静态层拷进来
  std::copy(sky_r_.begin(), sky_r_.end(), fr_.begin());
  std::copy(sky_g_.begin(), sky_g_.end(), fg_.begin());
  std::copy(sky_b_.begin(), sky_b_.end(), fb_.begin());
  std::copy(floor0_.begin(), floor0_.end(), floor_.begin());

  // 气辉沿地平线缓慢漂移
  const int air0 = static_cast<int>(std::floor(kHorizon * 0.3f));
  for (int r = air0; r < kHorizon + 2; ++r) {
    for (int x = 0; x < kWidth; ++x) {
      const int k = r * kWidth + x;
      if (mat_[k] != kSkyD)
        continue;
      float a = air_[k] *
                (0.86f + 0.28f * Noisef((x - tf * 0.3f) * 0.03f, r * 0.08f, 0));
      fr_[k] += a * 0.3f;
      fg_[k] += a * 0.5f;
      fb_[k] += a * 1.2f;
    }
  }

  // 沙丘、树
  for (int k = 0; k < N; ++k) {
    const int m = mat_[k];
    if (m == kSandD) {
      fr_[k] = sand_r_[k];
      fg_[k] = sand_g_[k];
      fb_[k] = sand_b_[k];
      const float c = crest_[k];
      if (c > 0.f) {
        // 沙脊上偶尔闪一下的星砂
        const float h = Hashf(k, 5);
        const float s = std::sin(tf * (0.8f + 2.2f * h) + h * 40.f);
        const float g = c * std::pow(std::max(0.f, s), 8.f) * 0.5f;
        fr_[k] += g;
        fg_[k] += g * 0.85f;
        fb_[k] += g * 0.7f;
      }
    } else if (m == kTreeD) {
      const float e = bark_[k];
      fr_[k] = e * 0.42f;
      fg_[k] = e * 0.3f;
      fb_[k] = e * 0.2f;
      floor_[k] = e > 0.f ? 0.1f : 0.f;
    }
  }

  // 星星
  for (const auto& st : stars_) {
    const float tw = st.bright > 0.35f
                         ? 0.72f + 0.28f * std::sin(tf * st.rate + st.phase)
                         : 0.6f + 0.4f * std::sin(tf * st.rate + st.phase);
    const float s = st.bright * tw;
    fr_[st.k] += s * st.tr;
    fg_[st.k] += s * st.tg;
    fb_[st.k] += s * st.tb;
    floor_[st.k] = 0.28f;
  }

  // 小镇灯光
  for (const auto& lp : lamps_) {
    const float g = 0.42f + 0.14f * std::sin(tf * lp.rate + lp.phase) *
                                std::sin(tf * lp.rate * 2.3f + lp.phase * 3.f);
    fr_[lp.k] = g;
    fg_[lp.k] = 0.85f * g;
    fb_[lp.k] = 0.66f * g;
    floor_[lp.k] = 0.12f;
  }

  // 流星：每 7 秒一颗，按槽位算
  const int n = static_cast<int>(std::floor((tf - 2.5f) / 7.f)) + 1;
  for (int i = std::max(0, n - 1); i <= n + 1; ++i) {
    float start, x0, y0, dx, dy, speed;
    MeteorParams(i, start, x0, y0, dx, dy, speed);
    const float age = tf - start;
    if (age < 0.f || age > 0.9f)
      continue;
    const float fadeIn = Smoothf(0.f, 0.12f, age);
    const float fadeOut = Smoothf(0.9f, 0.6f, age);
    const float hx = x0 + dx * speed * age;
    const float hy = y0 + dy * speed * age * 0.75f;
    const float len = std::min(28.f, speed * age);
    const int steps = static_cast<int>(len * 2.f);
    for (int j = 0; j < steps; ++j) {
      const float f = static_cast<float>(j) / static_cast<float>(steps);
      const int px = static_cast<int>(std::round(hx - dx * f * len));
      const int py = static_cast<int>(std::round(hy - dy * f * len * 0.75f));
      if (px < 0 || px >= kWidth || py < 0 || py >= kHeight)
        continue;
      const int k = py * kWidth + px;
      if (mat_[k] != kSkyD)
        continue;
      const float s = (1.f - f) * fadeIn * fadeOut * 1.25f;
      fr_[k] = std::max(fr_[k], s * 0.9f);
      fg_[k] = std::max(fg_[k], s * 0.96f);
      fb_[k] = std::max(fb_[k], s);
      floor_[k] = std::max(floor_[k], s > 0.12f ? 0.4f : 0.f);
    }
  }

  // ---------- 量化 ----------
  for (int r = 0; r < kHeight; ++r) {
    const float edge = Smoothf(kHeight + 1.f, kHeight - 12.f, r + 0.5f);
    for (int x = 0; x < kWidth; ++x) {
      const int k = r * kWidth + x;
      const float cr = fr_[k], cg = fg_[k], cbl = fb_[k], fl = floor_[k];
      const float peak = std::max({cr, cg, cbl, 1e-4f});
      const bool sky = (mat_[k] == kSkyD);
      const float level =
          Clampf(fl + (1.f - fl) * std::pow(peak, sky ? 0.92f : 0.75f)) * edge;
      const int step = std::max(
          0, std::min(3, static_cast<int>(std::round(level * 3.f + dith_[k]))));
      step_[k] = static_cast<uint8_t>(step);
      if (step) {
        const float want =
            std::min(1.f, (level + 0.06f) / kHalftoneCover[step]);
        const float s = (0.3f + 0.7f * want) / peak;
        color_[k] = static_cast<uint8_t>(
            palette_.Nearest(Clampf(cr * s), Clampf(cg * s), Clampf(cbl * s)));
      } else {
        color_[k] = 0;
      }
    }
  }
}