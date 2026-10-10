#include "marine_drive.h"

#include <algorithm>
#include <cmath>

#include <QColor>

// ============================== 常量 =======================================
static constexpr int kHorizon = 40;
static constexpr int kTip = 176;
static constexpr float kMoonX = 189.0f;
static constexpr float kMoonY = 13.0f;
static constexpr float kMoonR = 4.5f;
static constexpr int kBrokenLamp = 6;
static constexpr int kCloudW = 800;

static const float kMaria[3][3] = {
    {-1.4f, -1.2f, 1.6f},
    {1.3f, 0.8f, 1.4f},
    {-0.6f, 2.0f, 1.1f},
};
static const float kSodium[3] = {1.0f, 0.56f, 0.18f};

static const char* const kPaletteHex[MarineDrive::kPaletteN] = {
    "#18213f", "#202c55", "#2a396d", "#374a88", "#4a60a2", "#6880bc",
    "#2c2140", "#3f2d58", "#56396b", "#6e4a7a", "#8a5f88", "#5a2e3c",
    "#7b404c", "#9e5858", "#55300f", "#7d4515", "#a95c1b", "#d27a24",
    "#ee9631", "#ffb246", "#ffcd6a", "#ffe39c", "#fff2cf", "#ffffff",
    "#dce6ff", "#a9bde8", "#6a181b", "#b22a28", "#ff4b3e", "#132c38",
    "#1c3f4f", "#2a5868", "#29283a", "#3c3a4c", "#565266", "#79738a",
};

enum Mat {
  kSky = 0,
  kWater = 1,
  kFar = 2,
  kTower = 3,
  kFront = 4,
  kRoad = 5,
  kWall = 6,
  kPole = 7,
  kShip = 8
};

static inline float ShoreF(float x) {
  float t = std::max(0.f, 1.f - x / kTip);
  return 50.f + 34.f * std::pow(t, 2.2f);
}
static inline float ScF(float x) {
  float t = std::max(0.f, 1.f - x / kTip);
  return 0.5f + 1.9f * std::pow(t, 1.5f);
}
static inline float SeaTopF(float x) {
  return x <= kTip
             ? ShoreF(x)
             : std::max(static_cast<float>(kHorizon), 50.f - (x - kTip) * 1.6f);
}
static inline float WallTopF(float x) {
  return ShoreF(x) - std::max(1.f, ScF(x));
}
static inline float RoadTopF(float x) {
  return WallTopF(x) - std::max(1.f, 1.5f * ScF(x));
}
static inline float GlowF(float x, float y) {
  float over = 0.5f + 0.5f * Smoothf(kTip + 24, kTip - 16, x);
  float h = std::max(0.f, kHorizon - y);
  return over * (std::exp(-h / 4) * 0.5f + std::exp(-h / 12) * 0.12f);
}

// ============================== 公开接口 ===================================
const char* const* MarineDrive::PaletteHex() { return kPaletteHex; }

MarineDrive::MarineDrive() { Init(); }

// ============================== 静态构建 ===================================
void MarineDrive::Init() {
  palette_.Init(kPaletteHex, kPaletteN);

  const int N = kWidth * kHeight;
  mat_.assign(N, 0);
  lane_.assign(N, 0);
  s_r_.assign(N, 0);
  s_g_.assign(N, 0);
  s_b_.assign(N, 0);
  flo_.assign(N, 0);
  sky_glow_.assign(N, 0);
  lamp_i_.assign(N, 0);
  lamp_w_.assign(N, 0);
  flick_i_.assign(N, 0);
  flick_w_.assign(N, 0);
  for (auto& a : refl_)
    a.assign(N, 0);
  refl_fl_.assign(N, 0);
  cover_.assign(kCloudW * kHorizon, 0);
  clit_.assign(kCloudW * kHorizon, 0);
  clear_.assign(N, 0);
  step_.assign(N, 0);
  color_.assign(N, 0);

  // ---------- 弧长参数化 ----------
  u_tab_.assign(kCurveSamples, 0.f);
  for (int i = 1; i < kCurveSamples; ++i) {
    float x = (i - 0.5f) * kStepU;
    float sl = (ShoreF(x + 0.05f) - ShoreF(x - 0.05f)) / 0.1f;
    u_tab_[i] = u_tab_[i - 1] + (std::sqrt(1 + sl * sl) * kStepU) / ScF(x);
  }
  u_end_ = Uof(static_cast<float>(kTip));

  // ---------- 路灯 ----------
  for (float u = 0.6f; u < u_end_ - 0.4f; u += 3.75f) {
    float lx = XOfU(u), s = ScF(lx);
    lamps_.push_back({lx, WallTopF(lx) - 2.4f * s, s});
  }

  // ---------- 前景建筑 ----------
  for (float u = -4; u < u_end_;) {
    int i = static_cast<int>(blds_.size());
    float w = 7 + 7 * Hashf(i, 21);
    int tall = static_cast<int>(
        std::round(7 * Smoothf(26, 2, u) * (0.6f + 0.4f * Hashf(i, 26))));
    Building b;
    b.u0 = u;
    b.u1 = u + w;
    b.floors = 5 + static_cast<int>((Hashf(i, 22) * 4)) + tall;
    b.tone = 0.75f + 0.5f * Hashf(i, 23);
    b.crown = Hashf(i, 24) > 0.45f;
    blds_.push_back(b);
    u += w + 0.4f + 1.8f * Hashf(i, 25);
  }
  col_u_.fill(0);
  bld_at_.fill(-1);
  for (int x = 0; x < kWidth; ++x) {
    col_u_[x] = Uof(x + 0.5f);
    if (x + 0.5f > kTip)
      continue;
    for (int i = 0; i < static_cast<int>(blds_.size()); ++i)
      if (col_u_[x] >= blds_[i].u0 && col_u_[x] < blds_[i].u1)
        bld_at_[x] = static_cast<int16_t>(i);
  }

  // ---------- 塔楼 ----------
  struct Tower {
    float tx, hw;
    int top, warm;
  };
  static const Tower kTowers[] = {
      {24, 4.5f, 21, 0},  {97, 2.5f, 27, 1},     {107, 2, 22, 0},
      {118, 3, 30, 0},    {126, 2, 25, 1},       {136, 3, 23, 0},
      {142.5f, 2, 15, 1}, {149, 3.5f, 27, 0},    {156, 2.5f, 19, 0},
      {162, 3, 12, 1},    {168.5f, 2.5f, 21, 0}, {173, 2, 28, 1},
  };
  const int kTowerCount =
      static_cast<int>((sizeof(kTowers) / sizeof(kTowers[0])));

  // ---------- 主静态逐格循环 ----------
  for (int r = 0; r < kHeight; ++r) {
    for (int x = 0; x < kWidth; ++x) {
      const int k = r * kWidth + x;
      const float xc = x + 0.5f, y = r + 0.5f;

      if (y >= SeaTopF(xc)) {
        mat_[k] = kWater;
        continue;
      }

      int m = kSky;
      float cr = 0, cg = 0, cb = 0, fl = 0.15f;

      // 远景城市
      int blk =
          static_cast<int>(std::floor((x + 40 * Fbmf(x * 0.02f, 5, 2, 0)) / 3));
      float h7 = Hashf(blk, 7);
      float farTop = kHorizon - 0.4f -
                     (0.4f + 5.f * h7 * h7 * h7) *
                         (0.5f + 0.5f * Fbmf(x * 0.05f, 2, 2, 0));
      if (xc < kTip + 7 && y >= farTop) {
        m = kFar;
        float g = GlowF(xc, kHorizon - 2);
        cr = 0.03f + 0.05f * g;
        cg = 0.04f + 0.03f * g;
        cb = 0.08f + 0.02f * g;
        if (Hashf(x * 3 + 1, r * 7 + 2) < 0.035f) {
          float v = 0.25f + 0.45f * Hashf(x, r * 5 + 9);
          bool cool = Hashf(x * 5, r) < 0.3f;
          cr += v * (cool ? 0.8f : 1.f);
          cg += v * (cool ? 0.85f : 0.7f);
          cb += v * (cool ? 1.f : 0.4f);
        }
        fl = 0.08f;
      }

      // 塔楼
      for (int ti = 0; ti < kTowerCount; ++ti) {
        const auto& t = kTowers[ti];
        float dx = xc - t.tx;
        if (std::abs(dx) > t.hw || y < t.top)
          continue;
        m = kTower;
        cr = 0.015f;
        cg = 0.018f;
        cb = 0.035f;
        if (dx > t.hw - 1) {
          cr += 0.04f;
          cg += 0.055f;
          cb += 0.10f;
        }
        float g = GlowF(xc, y) * 0.2f;
        cr += g * 0.42f;
        cg += g * 0.21f;
        cb += g * 0.10f;
        int fr = r - t.top;
        if (fr == 0) {
          cr += 0.08f;
          cg += 0.10f;
          cb += 0.16f;
        } else if (fr > 0 && (fr & 1) && std::abs(dx) < t.hw - 0.5f) {
          int seg = static_cast<int>(std::floor((xc - t.tx + t.hw) / 3));
          float h = Hashf(ti * 17 + seg, r * 13 + 3);
          if (h < 0.5f) {
            float v = 0.4f + 0.3f * Hashf(ti * 7 + seg, r);
            bool cool = t.warm ? (h < 0.1f) : (h > 0.1f);
            cr += v * (cool ? 0.72f : 1.00f);
            cg += v * (cool ? 0.84f : 0.72f);
            cb += v * (cool ? 1.00f : 0.42f);
          }
        }
        if (fr == 0 && std::abs(dx) < 0.6f && t.top < 20)
          beacons_.push_back({k - kWidth, ti * 1.7f});
        fl = 0.03f;
      }

      // 前景
      if (xc <= kTip) {
        float s = ScF(xc);
        float wt = WallTopF(xc);
        float rt = RoadTopF(xc);
        int bi = bld_at_[x];

        if (bi >= 0 && y < rt) {
          const Building& b = blds_[bi];
          float uu = col_u_[x] - b.u0, bw = b.u1 - b.u0;
          float f = (rt - y) / s;
          float hgt = b.floors * 1.7f + 1.1f;
          if (b.crown && uu > bw * 0.32f && uu < bw * 0.68f)
            hgt += 1.6f;
          if (f < hgt) {
            m = kFront;
            bool edge = (uu < 0.5f) || (uu > bw - 0.5f);
            float t0 = b.tone * (edge ? 1.35f : 1.f);
            cr = 0.03f * t0;
            cg = 0.035f * t0;
            cb = 0.06f * t0;
            float up = std::exp(-f / 2) * 0.16f;
            cr += up * kSodium[0];
            cg += up * kSodium[1];
            cb += up * kSodium[2];
            if (f > hgt - 0.6f / s || (f > b.floors * 1.7f + 0.6f &&
                                       f < b.floors * 1.7f + 0.6f + 0.5f / s)) {
              cr += 0.10f;
              cg += 0.13f;
              cb += 0.22f;
            }
            if (uu > bw - 0.7f / s - 0.2f) {
              cr += 0.04f;
              cg += 0.06f;
              cb += 0.12f;
            }
            int fi = static_cast<int>(std::floor((f - 0.9f) / 1.7f));
            float ff = (f - 0.9f) - fi * 1.7f;
            int col = static_cast<int>(std::floor((uu - 0.3f) / 1.5f));
            float cu = uu - 0.3f - col * 1.5f;
            bool fine = s * 1.7f >= 2.6f;
            bool inWin =
                fi >= 0 && fi < b.floors && uu > 0.7f && uu < bw - 0.7f &&
                (!fine ||
                 (ff > 0.45f && ff < 1.35f && cu > 0.35f && cu < 1.15f));
            if (inWin && !fine) {
              float hh =
                  Hashf(bi * 13 + static_cast<int>(std::floor(uu / 3.2f)),
                        fi * 7 + 1);
              float v = 0.04f + 0.26f * hh * hh;
              cr += v;
              cg += v * 0.8f;
              cb += v * 0.55f;
              if (Hashf(x * 7 + 3, r * 11) < 0.035f) {
                cr += 0.4f;
                cg += 0.36f;
                cb += 0.28f;
              }
            } else if (inWin) {
              float h = Hashf(bi * 97 + col, fi * 31 + 5);
              bool lit = h < (fine ? 0.3f : 0.2f);
              if (lit) {
                float v = (0.35f + 0.3f * Hashf(bi * 13 + col, fi)) *
                          (fine ? 1.f : 0.7f);
                float kind = Hashf(bi * 7 + col, fi * 3 + 1);
                float wr = 1.f, wg = 0.84f, wb = 0.58f;
                if (kind < 0.25f) {
                  wr = 0.82f;
                  wg = 0.90f;
                  wb = 1.f;
                }
                cr += v * wr;
                cg += v * wg;
                cb += v * wb;
                float fk = Hashf(bi * 5 + col, fi * 11 + 7);
                if (fk < 0.07f)
                  flick_.push_back({k, v * wr, v * wg, v * wb,
                                    fk < 0.025f ? 1 : 0, 0.3f + fk * 9.f,
                                    fk * 400.f});
              } else if (fine) {
                cr *= 0.7f;
                cg *= 0.7f;
                cb *= 0.8f;
              }
            }
            fl = 0.03f;
          }
        }

        if (y >= rt && y < wt) {
          m = kRoad;
          cr = 0.07f;
          cg = 0.05f;
          cb = 0.05f;
          fl = 0.1f;
          float lf = (y - rt) / (wt - rt);
          lane_[k] = (wt - rt < 1.8f) ? 3 : (lf < 0.5f ? 1 : 2);
        } else if (y >= wt) {
          m = kWall;
          cr = 0.10f;
          cg = 0.09f;
          cb = 0.10f;
          fl = 0.1f;
        }
        for (const auto& L : lamps_) {
          if (L.s > 1.15f && std::abs(xc - L.x) < 0.5f && y > L.y && y < wt) {
            m = kPole;
            cr = 0.09f;
            cg = 0.08f;
            cb = 0.09f;
            fl = 0.10f;
          }
        }
      }

      // 天空
      if (m == kSky) {
        float v = y / kHorizon;
        float g = GlowF(xc, y) * (1.f + 0.6f * Smoothf(84, 98, xc) *
                                            Smoothf(186, 174, xc)) +
                  0.2f * std::exp(-std::max(0.f, kHorizon - y) / 14.f) *
                      Smoothf(84, 104, xc) * Smoothf(188, 172, xc);
        float veil = 0.88f + 0.24f * Fbmf(x * 0.05f, r * 0.08f, 3, 0);
        float vv = std::pow(v, 1.4f);
        cr = (0.03f + 0.07f * vv) * veil + g * 0.42f;
        cg = (0.05f + 0.12f * vv) * veil + g * 0.22f;
        cb = (0.15f + 0.24f * vv) * veil + g * 0.10f;
        float hzn = std::exp(-std::max(0.f, kHorizon - y) / 3.5f) *
                    Smoothf(172, 186, xc) * 0.16f;
        cr += hzn * 0.60f;
        cg += hzn * 0.70f;
        cb += hzn * 0.95f;
        float dmx = xc - kMoonX, dmy = y - kMoonY;
        float dm = std::sqrt(dmx * dmx + dmy * dmy);
        float halo = std::exp(-dm / 22.f) * 0.13f + std::exp(-dm / 6.f) * 0.26f;
        cr += halo * 0.90f;
        cg += halo * 0.86f;
        cb += halo * 0.80f;
        float ring = 0.03f * Smoothf(4.6f, 5.6f, dm) * Smoothf(8.5f, 6.5f, dm);
        cr -= ring;
        cg -= ring;
        cb -= ring * 0.8f;
        if (dm < kMoonR) {
          float face = 0.9f + 0.1f * Fbmf(x * 0.6f, r * 0.6f, 2, 0);
          for (const auto& ma : kMaria) {
            float dx = dmx - ma[0], dy = dmy - ma[1];
            float d2 = (dx * dx + dy * dy) / (ma[2] * ma[2]);
            if (d2 < 1)
              face -= 0.14f * (1 - d2);
          }
          float a = Smoothf(kMoonR, kMoonR - 0.8f, dm);
          cr = Mixf(cr, face, a);
          cg = Mixf(cg, face * 0.95f, a);
          cb = Mixf(cb, face * 0.86f, a);
        }
        sky_glow_[k] = g;
      }

      mat_[k] = static_cast<uint8_t>(m);
      s_r_[k] = cr;
      s_g_[k] = cg;
      s_b_[k] = cb;
      flo_[k] = fl;
    }
  }

  // ---------- 船只 ----------
  struct ShipLight {
    int x, r;
    float cr, cg, cb;
  };
  static const ShipLight kShipLights[] = {
      {184, 35, 1, 1, 1},
      {190, 34, 1, 1, 1},
      {185, 37, 1, 0.78f, 0.45f},
      {187, 37, 1, 0.8f, 0.5f},
      {189, 36, 1, 0.82f, 0.5f},
      {191, 36, 1, 0.8f, 0.5f},
      {197, 36, 1, 1, 1},
      {196, 38, 1, 0.78f, 0.45f},
      {199, 38, 0.95f, 0.22f, 0.18f},
  };
  auto hull = [](int x, int r) {
    return (r >= 38 && x >= 182 && x <= 192) ||
           (r == 37 && x >= 183 && x <= 192) ||
           (r >= 35 && r <= 36 && x >= 189 && x <= 191) ||
           (r >= 35 && r <= 36 && x == 184) ||
           (r >= 38 && x >= 195 && x <= 199) ||
           (r == 37 && x >= 196 && x <= 197);
  };
  for (int r = 34; r < 40; ++r)
    for (int x = 180; x < kWidth; ++x)
      if (hull(x, r)) {
        int k = r * kWidth + x;
        mat_[k] = kShip;
        s_r_[k] = 0.002f;
        s_g_[k] = 0.002f;
        s_b_[k] = 0.004f;
        flo_[k] = 0;
      }
  for (const auto& sl : kShipLights) {
    int k = sl.r * kWidth + sl.x;
    mat_[k] = kShip;
    s_r_[k] = sl.cr;
    s_g_[k] = sl.cg;
    s_b_[k] = sl.cb;
    flo_[k] = 0.1f;
  }

  // ---------- 灯光传播 + 倒影 ----------
  for (int j = 0; j < static_cast<int>(lamps_.size()); ++j) {
    const auto& L = lamps_[j];
    float core = 0.34f + 0.30f * L.s;
    float halo_ = 0.40f + 0.40f * L.s;
    float spill = 1.20f + 1.50f * L.s;
    int R = static_cast<int>(std::ceil(spill * 3.2f));

    auto& into = (j == kBrokenLamp) ? flick_i_ : lamp_i_;
    auto& hot = (j == kBrokenLamp) ? flick_w_ : lamp_w_;

    for (int r = std::max(0, static_cast<int>(L.y) - R);
         r < std::min(kHeight, static_cast<int>(L.y) + R); ++r)
      for (int x = std::max(0, static_cast<int>(L.x) - R);
           x < std::min(kWidth, static_cast<int>(L.x) + R); ++x) {
        int k = r * kWidth + x;
        if (mat_[k] == kWater)
          continue;
        float dx = x + 0.5f - L.x, dy = r + 0.5f - L.y;
        float d = std::sqrt(dx * dx + dy * dy);
        float c = std::exp(-std::pow(d / core, 2.f)) * 2.2f;
        into[k] += c * 0.7f + std::exp(-d / halo_) * 0.24f +
                   std::exp(-d / spill) * 0.02f;
        hot[k] += c * 0.3f;
      }

    float sy = ShoreF(L.x);
    float wj = 0.4f + 0.7f * L.s, len = 2.f + 8.f * L.s;
    for (int r = static_cast<int>(sy); r < kHeight; ++r) {
      float dy = r + 0.5f - sy;
      if (dy < 0)
        continue;
      float a = std::exp(-dy / (len * 1.7f)) * (0.3f + 0.15f * L.s) *
                Smoothf(-0.5f, 1.5f, dy);
      if (a < 0.004f)
        break;
      for (int x = std::max(0, static_cast<int>((L.x - 3 * wj - 1)));
           x < std::min(kWidth, static_cast<int>((L.x + 3 * wj + 1))); ++x) {
        int k = r * kWidth + x;
        if (mat_[k] != kWater)
          continue;
        float g = a * std::exp(-std::pow((x + 0.5f - L.x) / wj, 2.f));
        if (j == kBrokenLamp)
          refl_fl_[k] += g;
        else {
          refl_[0][k] += g * kSodium[0];
          refl_[1][k] += g * kSodium[1];
          refl_[2][k] += g * kSodium[2];
        }
      }
    }
  }

  for (int k = 0; k < N; ++k) {
    int m = mat_[k];
    if (m == kWater || m == kSky)
      continue;
    s_r_[k] += lamp_i_[k] * kSodium[0] + lamp_w_[k];
    s_g_[k] += lamp_i_[k] * kSodium[1] + lamp_w_[k] * 0.95f;
    s_b_[k] += lamp_i_[k] * kSodium[2] + lamp_w_[k] * 0.80f;
  }

  for (const auto& sl : kShipLights)
    for (int r = kHorizon; r < kHorizon + 14; ++r) {
      float a = std::exp(-(r - kHorizon) / 5.f) * 0.35f;
      for (int dx = -1; dx <= 1; ++dx) {
        int xx = sl.x + dx;
        if (xx >= kWidth || xx < 0)
          continue;
        int k = r * kWidth + xx;
        if (mat_[k] != kWater)
          continue;
        float g = a * std::exp(-dx * dx * 2.5f);
        refl_[0][k] += g * sl.cr;
        refl_[1][k] += g * sl.cg;
        refl_[2][k] += g * sl.cb;
      }
    }

  // 镜面
  for (int r = 0; r < kHeight; ++r)
    for (int x = 0; x < kWidth; ++x) {
      int k = r * kWidth + x;
      if (mat_[k] != kWater)
        continue;
      float sy = SeaTopF(x + 0.5f), d = r + 0.5f - sy;
      float ar = 0, ag = 0, ab = 0;
      int n = 0;
      for (int o = 0; o < 3; ++o) {
        int ym = static_cast<int>(std::floor(sy - d * 0.55f - 0.3f - o * 0.7f));
        if (ym < 0)
          continue;
        int q = ym * kWidth + x;
        if (mat_[q] == kWater)
          continue;
        ++n;
        if (mat_[q] == kSky) {
          ar += s_r_[q];
          ag += s_g_[q];
          ab += s_b_[q];
        } else if (mat_[q] == kTower || mat_[q] == kShip) {
          ar += std::max(0.f, s_r_[q] - 0.25f) * 0.6f;
          ag += std::max(0.f, s_g_[q] - 0.25f) * 0.6f;
          ab += std::max(0.f, s_b_[q] - 0.25f) * 0.6f;
        }
      }
      float a = 0.32f * std::exp(-d / 30.f);
      if (n) {
        refl_[0][k] += (ar / n) * a;
        refl_[1][k] += (ag / n) * a;
        refl_[2][k] += (ab / n) * a;
      }
      if (x + 0.5f <= kTip + 4) {
        float sh =
            0.025f * std::exp(-d / 5.f) * Smoothf(kTip + 4, kTip - 6, x + 0.5f);
        refl_[0][k] += sh * kSodium[0];
        refl_[1][k] += sh * kSodium[1];
        refl_[2][k] += sh * kSodium[2];
      }
    }

  // ---------- 云层 ----------
  auto density = [](float x, float y) {
    float q = Fbmf(x * 0.010f, y * 0.05f, 3, 8);
    float d = Fbmf(x * 0.025f + q * 2.6f, y * 0.07f + q * 1.3f, 5, 20);
    float yb = y + 14.f * (Fbmf(x * 0.005f + 3, 7, 2, 4) - 0.5f);
    return d + 0.05f * Smoothf(2, 14, yb) - 0.10f * Smoothf(6, 0, y) -
           0.10f * Smoothf(22, kHorizon - 4, yb);
  };
  for (int r = 0; r < kHorizon; ++r) {
    for (int x = 0; x < kCloudW; ++x) {
      float y = r + 0.5f;
      float d = density(static_cast<float>(x), y);
      cover_[r * kCloudW + x] = Smoothf(0.47f, 0.62f, d) * 0.9f;
      clit_[r * kCloudW + x] = Clampf(
          0.5f + (d - density(x - 1.f, y + 2.5f)) * 9.f - (d - 0.6f) * 1.4f);
    }
  }

  for (int k = 0; k < N; ++k) {
    float x = (k % kWidth) + 0.5f, y = static_cast<float>((k / kWidth)) + 0.5f;
    clear_[k] = 1.f - 0.9f * Smoothf(84, 96, x) * Smoothf(186, 176, x) *
                          Smoothf(20, 28, y);
  }

  // ---------- 车辆 ----------
  bin_count_ = static_cast<int>(std::ceil((u_end_ + 12) / kBinWidth));
  lane_a_.assign(bin_count_, 0);
  lane_b_.assign(bin_count_, 0);
  for (int i = 0; i < 26; ++i)
    cars_.push_back(
        {Hashf(i, 51) * (u_end_ + 12), 4.2f + 1.6f * Hashf(i, 52), i & 1});
  for (int x = 0; x < kWidth; ++x) {
    bin_lo_[x] = std::max(
        0, static_cast<int>(std::floor(
               (Uof(static_cast<float>(x)) - kCarStartU) / kBinWidth)));
    bin_hi_[x] = std::min(
        bin_count_ - 1,
        std::max(bin_lo_[x], static_cast<int>(std::floor(
                                 (Uof(static_cast<float>(x) + 1) - kCarStartU) /
                                 kBinWidth))));
  }
}

// ---------- 参数化查找 -----------------------------------------------------
float MarineDrive::Uof(float x) const {
  float f = std::max(
      0.f, std::min(static_cast<float>(kCurveSamples) - 1.001f, x / kStepU));
  int i = static_cast<int>(f);
  return u_tab_[i] + (u_tab_[i + 1] - u_tab_[i]) * (f - i);
}
float MarineDrive::XOfU(float u) const {
  float lo = 0, hi = static_cast<float>(kTip);
  for (int i = 0; i < 30; ++i) {
    float m = (lo + hi) * 0.5f;
    if (Uof(m) < u)
      lo = m;
    else
      hi = m;
  }
  return (lo + hi) * 0.5f;
}

// ---------- 单格覆盖 -------------------------------------------------------
void MarineDrive::PaintCell(int k, float cr, float cg, float cb,
                            float base_level) {
  cr = std::max(0.f, cr);
  cg = std::max(0.f, cg);
  cb = std::max(0.f, cb);
  int r = k / kWidth, x = k % kWidth;
  float peak = std::max({cr, cg, cb, 1e-4f});
  float level =
      Clampf(base_level + (1 - base_level) * std::pow(peak, 0.85f) * 0.95f);
  int step = std::max(
      0,
      std::min(3, static_cast<int>(std::round(
                      level * 3.f + kHalftoneBayer[(r & 3) * 4 + (x & 3)]))));
  step_[k] = static_cast<uint8_t>(step);
  if (step) {
    float want = std::min(1.f, (level + 0.06f) / kHalftoneCover[step]);
    float s = (0.3f + 0.7f * want) / peak;
    color_[k] = static_cast<uint8_t>(
        palette_.Nearest(Clampf(cr * s), Clampf(cg * s), Clampf(cb * s)));
  } else {
    color_[k] = 0;
  }
}

// ---------- 每帧 -----------------------------------------------------------
void MarineDrive::Update(double t) {
  const float tf = static_cast<float>(t);
  const float drift = tf * 1.3f;

  // 车辆
  std::fill(lane_a_.begin(), lane_a_.end(), 0.f);
  std::fill(lane_b_.begin(), lane_b_.end(), 0.f);
  const float span = u_end_ + 12.f;
  for (const auto& c : cars_) {
    float p = std::fmod(c.p0 + (c.dir ? -c.v : c.v) * tf, span);
    if (p < 0)
      p += span;
    auto& into = c.dir ? lane_b_ : lane_a_;
    int head = static_cast<int>((p / kBinWidth));
    for (int i = 0; i < 26; ++i) {
      int b = c.dir ? head + i : head - i;
      if (b < 0 || b >= bin_count_)
        continue;
      float e = std::exp(-i / 7.f) * (i < 2 ? 1.3f : 0.85f);
      if (e > into[b])
        into[b] = e;
    }
  }
  for (int x = 0; x < kWidth; ++x) {
    float a = 0, b = 0;
    for (int i = bin_lo_[x]; i <= bin_hi_[x]; ++i) {
      a = std::max(a, lane_a_[i]);
      b = std::max(b, lane_b_[i]);
    }
    col_a_[x] = a;
    col_b_[x] = b;
  }

  // 坏掉的那盏
  bool sputter = std::sin(tf * 0.7f) > 0.45f;
  float lampOn =
      sputter
          ? (Hashf(static_cast<int>(std::floor(tf * 9.f)), 77) > 0.45f ? 1.f
                                                                       : 0.15f)
          : 1.f;

  // 主循环
  for (int r = 0; r < kHeight; ++r) {
    const float y = r + 0.5f;
    for (int x = 0; x < kWidth; ++x) {
      const int k = r * kWidth + x;
      const int m = mat_[k];
      float cr = s_r_[k], cg = s_g_[k], cb = s_b_[k], fl = flo_[k], fade = 1.f;

      if (m == kSky) {
        if (r < kHorizon) {
          float sx = x + drift, ix = std::floor(sx), fx = sx - ix;
          int ii = static_cast<int>(ix);
          int i0 = r * kCloudW + ((ii % kCloudW) + kCloudW) % kCloudW;
          int i1 = r * kCloudW + (((ii + 1) % kCloudW) + kCloudW) % kCloudW;
          float c = (cover_[i0] + (cover_[i1] - cover_[i0]) * fx) * clear_[k];
          if (c > 0.01f) {
            float l = clit_[i0] + (clit_[i1] - clit_[i0]) * fx;
            float g = sky_glow_[k];
            float dmx = x + 0.5f - kMoonX, dmy = y - kMoonY;
            float dm = std::sqrt(dmx * dmx + dmy * dmy);
            float nE = std::exp(-dm / 16.f);
            float b = Clampf(0.1f + l * (0.42f + 0.9f * g + 0.5f * nE));
            float kr = b < 0.5f ? Mixf(0.05f, 0.22f, b * 2.f)
                                : Mixf(0.22f, 0.90f, b * 2.f - 1.f);
            float kg = b < 0.5f ? Mixf(0.06f, 0.26f, b * 2.f)
                                : Mixf(0.26f, 0.88f, b * 2.f - 1.f);
            float kb = b < 0.5f ? Mixf(0.15f, 0.44f, b * 2.f)
                                : Mixf(0.44f, 0.92f, b * 2.f - 1.f);
            float aa = std::min(1.f, c * 1.1f) *
                       Smoothf(kMoonR, kMoonR + 4, dm) *
                       (1.f - 0.8f * Smoothf(27, 37, y));
            cr = Mixf(cr, kr, aa);
            cg = Mixf(cg, kg, aa);
            cb = Mixf(cb, kb, aa);
          }
        }
        if (y < kHorizon - 14 && Hashf(x, r * 3 + 11) > 0.993f) {
          float tw = 0.3f + 0.2f * std::sin(tf * (1.3f + Hashf(x, r) * 2.5f) +
                                            Hashf(r, x) * 6.28f);
          cr = std::max(cr, tw * 0.9f);
          cg = std::max(cg, tw * 0.9f);
          cb = std::max(cb, tw);
        }
        float li = lamp_i_[k], lw = lamp_w_[k];
        cr += li * kSodium[0] + lw;
        cg += li * kSodium[1] + lw * 0.95f;
        cb += li * kSodium[2] + lw * 0.80f;
        fl = 0.14f;
      } else if (m == kWater) {
        float v = (y - kHorizon) / (kHeight - kHorizon);
        float w =
            0.6f * Noisef(x * 0.06f + tf * 0.1f, y * 0.5f - tf * 0.5f, 0) +
            0.4f * Noisef(x * 0.18f - tf * 0.2f, y * 1.0f - tf * 1.0f, 0);
        float swell = 0.45f + 0.95f * w;
        float open = 0.85f + 0.30f * (x / static_cast<float>(kWidth));
        cr = (0.05f - 0.010f * v) * swell * open;
        cg = (0.095f - 0.015f * v) * swell * open;
        cb = (0.23f - 0.030f * v) * swell * open;
        {
          float denom = 0.8f + (y - kHorizon) * 0.12f;
          float road = std::exp(-std::pow((x + 0.5f - kMoonX) / denom, 2.f));
          float glint =
              Smoothf(0.52f, 0.80f,
                      0.45f * w + 0.55f * Noisef(x * 0.3f + tf * 0.3f,
                                                 y * 1.4f - tf * 1.4f, 0)) *
              road;
          cr += 0.90f * glint + 0.030f * road;
          cg += 0.88f * glint + 0.035f * road;
          cb += 0.80f * glint + 0.050f * road;
        }
        float wob = (Noisef(x * 0.05f + 7, y * 0.32f - tf * 0.7f, 0) - 0.5f) *
                    (1.4f + 4.5f * v);
        float sxr = x + wob;
        if (sxr < 0)
          sxr = 0;
        if (sxr > kWidth - 1.001f)
          sxr = kWidth - 1.001f;
        int i0r = r * kWidth + static_cast<int>(sxr);
        float fxr = sxr - static_cast<int>(sxr);
        float dash = 0.45f + 0.9f * Smoothf(0.20f, 0.55f,
                                            Noisef(x * 0.16f + 3,
                                                   y * 0.85f - tf * 1.3f, 0));
        auto ref = [&](const std::vector<float>& a) {
          return (a[i0r] + (a[i0r + 1] - a[i0r]) * fxr) * dash;
        };
        float f2 = ref(refl_fl_) * lampOn;
        float rr = ref(refl_[0]) + f2 * kSodium[0];
        float rg = ref(refl_[1]) + f2 * kSodium[1];
        float rb = ref(refl_[2]) + f2 * kSodium[2];
        float kill = 1.f - 0.9f * Clampf((rr - rb) * 3.f);
        cr = cr * kill + rr;
        cg = cg * kill + rg;
        cb = cb * kill + rb;
        fl = 0.16f;
        fade = Smoothf(kHeight + 6, kHeight - 8, y);
      } else if (m == kRoad) {
        int L = lane_[k];
        float a = (L & 1) ? col_a_[x] : 0.f;
        float b = (L & 2) ? col_b_[x] : 0.f;
        cr += a * 0.80f + b * 1.00f;
        cg += a * 0.10f + b * 0.90f;
        cb += a * 0.08f + b * 0.72f;
      }

      if (m != kWater) {
        float f = flick_i_[k] * lampOn;
        float fw = flick_w_[k] * lampOn;
        if (f) {
          cr += f * kSodium[0] + fw;
          cg += f * kSodium[1] + fw * 0.95f;
          cb += f * kSodium[2] + fw * 0.80f;
        }
      }

      float peak = std::max({cr, cg, cb, 1e-4f});
      float level =
          Clampf(fl + (1 - fl) * std::pow(peak, 0.85f) * 0.95f) * fade;
      int step = std::max(
          0, std::min(
                 3, static_cast<int>(std::round(
                        level * 3.f + kHalftoneBayer[(r & 3) * 4 + (x & 3)]))));
      step_[k] = static_cast<uint8_t>(step);
      if (step) {
        float want = std::min(1.f, (level + 0.06f) / kHalftoneCover[step]);
        float s = (0.3f + 0.7f * want) / peak;
        color_[k] = static_cast<uint8_t>(
            palette_.Nearest(Clampf(cr * s), Clampf(cg * s), Clampf(cb * s)));
      } else {
        color_[k] = 0;
      }
    }
  }

  // 房间闪烁 & 信标
  for (const auto& fc : flick_) {
    int k = fc.k;
    if (fc.kind) {
      float f = 0.5f + 0.5f * std::sin(tf * 11.f + fc.phase) *
                           std::sin(tf * 4.3f + fc.phase * 2.f);
      PaintCell(k, s_r_[k] - fc.wr + f * fc.wr * 0.45f,
                s_g_[k] - fc.wg + f * fc.wr * 0.60f,
                s_b_[k] - fc.wb + f * fc.wr, 0.08f);
    } else {
      float f = (std::sin(tf * fc.rate * 0.5f + fc.phase) > -0.6f) ? 1.f : 0.f;
      PaintCell(k, s_r_[k] - fc.wr * (1 - f), s_g_[k] - fc.wg * (1 - f),
                s_b_[k] - fc.wb * (1 - f), 0.08f);
    }
  }
  for (const auto& bp : beacons_) {
    int k = bp.first;
    if (k < 0)
      continue;
    bool on = std::sin(tf * 2.4f + bp.second) > 0.3f;
    PaintCell(k, on ? 1.00f : 0.30f, on ? 0.20f : 0.06f, on ? 0.15f : 0.05f,
              0.10f);
  }
}