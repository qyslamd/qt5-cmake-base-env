#include "rocket.h"

#include <algorithm>
#include <cmath>

// ============================== 常量 =======================================
static constexpr int kPad = Rocket::kHeight - 2;  // 26：发射台甲板行
static constexpr int kLoop = 18;                  // 循环周期（秒）
static constexpr float kArmT = 0.8f;              // 服务臂开始后摆
static constexpr float kIgniteT = 1.5f;           // 点火
static constexpr float kLiftT = 3.0f;             // 夹钳松开
static constexpr float kClimb = 2.6f;             // 上升加速度：行/秒²
static constexpr float kOut = 40.0f;              // 出画高度
static constexpr float kLandT = 14.6f;            // 着陆时刻
static constexpr float kDecel = 4.4f;             // 着陆点火减速度
static constexpr float kRate = 36.0f;             // 尾焰喷团：每秒个数
static const float kBurn = std::sqrt(2.0f * kOut / kDecel);  // ≈4.264

static constexpr int kLeft = (Rocket::kWidth - 14) / 2;  // 17
static constexpr float kMid = kLeft + 14 / 2.0f - 0.5f;  // 23.5
static constexpr int kTower = kLeft - 8;                 // 9
static constexpr int kArmRow = kPad - 21 + 8;            // 13

// clang-format off
static const char* const kRocketArt[] = {
    "      /\\      ",
    "     /ww\\     ",
    "    /wwww\\    ",
    "   /wwwwww\\   ",
    "   |wwwwww|   ",
    "   |wwwwww|   ",
    "   \\wwwwww/   ",
    "    |wwww|    ",
    "    |wwww|    ",
    "   /kkkkkk\\   ",
    "   |wwwwww|   ",
    "   |wwwwww|   ",
    "   |wwwwww|   ",
    "   |wwwwww|   ",
    "   |wwwwww|   ",
    "  /|wwwwww|\\  ",
    " /f|wwwwww|f\\ ",
    "/ff|wwwwww|ff\\",
    "    \\gggg/    ",
    "    /gggg\\    ",
    "   /gggggg\\   ",
};
// clang-format on

static const char* const kPaletteHex[Rocket::kPaletteN] = {
    "#c8c8d8",  // 0 主前景：火箭、塔架、烟雾、星
    "#ff9040",  // 1 火焰
};

// ============================== RNG ========================================
class Mulberry32 {
 public:
  explicit Mulberry32(uint32_t seed) : a_(seed) {}
  float Next() {
    a_ += 0x6d2b79f5u;
    uint32_t t = (a_ ^ (a_ >> 15)) * (1u | a_);
    t = (t + (t ^ (t >> 7)) * (61u | t)) ^ t;
    return static_cast<float>(t ^ (t >> 14)) / 4294967296.0f;
  }

 private:
  uint32_t a_;
};

// ============================== 时间函数 ===================================
static float HeightAt(float t) {
  if (t < kLiftT || t >= kLandT)
    return 0.f;
  float up = 0.5f * kClimb * (t - kLiftT) * (t - kLiftT);
  if (up < kOut)
    return up;
  float left = kLandT - t;
  return (left < kBurn) ? 0.5f * kDecel * left * left : -1.f;
}
static bool Burning(float t) {
  return t >= kIgniteT && t < kLandT && HeightAt(t) >= 0.f;
}
static bool OnPad(float t) { return t < kArmT - 0.3f || t >= kLandT + 1.f; }

// ============================== 公开接口 ===================================
const char* const* Rocket::PaletteHex() { return kPaletteHex; }

Rocket::Rocket() { Init(); }

// ============================== 静态构建 ===================================
void Rocket::Init() {
  const int N = kWidth * kHeight;
  chars_.assign(N, QChar(' '));
  colors_.assign(N, 0);
  fog_.assign(N, 0.f);

  rkt_char_.assign(kRocketW * kRocketH, QChar(' '));
  rkt_color_.assign(kRocketW * kRocketH, 0);

  // ---------- 预渲染火箭精灵 ----------
  // 圆柱光照：塔架泛光灯从左边来。字符直接用 BODY 斜坡，
  // 不做 step/color 映射，形状保留在字符里。
  const char* BODY = " .:-=+*#%";
  for (int r = 0; r < kRocketH; ++r) {
    const char* line = kRocketArt[r];

    // 本行第一个和最后一个 w/k/g 的位置
    int a = -1, b = -1;
    for (int c = 0; c < kRocketW; ++c) {
      char ch = line[c];
      if (ch == 'w' || ch == 'k' || ch == 'g') {
        if (a < 0)
          a = c;
        b = c + 1;
      }
    }

    // 本行首尾非空列
    int lo = -1, hi = -1;
    for (int c = 0; c < kRocketW; ++c) {
      if (line[c] != ' ') {
        if (lo < 0)
          lo = c;
        hi = c + 1;
      }
    }
    span_lo_[r] = lo < 0 ? 0 : lo;
    span_hi_[r] = hi < 0 ? 0 : hi;

    for (int c = 0; c < kRocketW; ++c) {
      char ch = line[c];
      QChar qch;
      uint8_t col = 0;

      if (ch == 'w' || ch == 'k' || ch == 'g' || ch == 'f') {
        float denom = (a < 0 || b < 0) ? 1.0f : (b - a) / 2.0f + 0.5f;
        float x = (c + 0.5f - kRocketW / 2.0f) / denom;
        float lit;
        if (ch == 'f') {
          lit = (x < 0.f) ? 0.75f : 0.3f;
        } else {
          lit = std::max(
              0.f, -0.72f * x + 0.69f * std::sqrt(std::max(0.f, 1.f - x * x)));
        }
        float tone = (ch == 'w')   ? 1.00f
                     : (ch == 'k') ? 0.42f
                     : (ch == 'g') ? 0.62f
                                   : 0.60f;
        int i = std::max(
            1,
            static_cast<int>(std::round(tone * (0.12f + 0.88f * lit) * 8.0f)));
        qch = QChar(BODY[i]);
      } else if (ch == ' ') {
        qch = QChar(' ');
      } else {
        // / \ | 直接保留
        qch = QChar(ch);
      }

      int k = r * kRocketW + c;
      rkt_char_[k] = qch;
      rkt_color_[k] = col;
    }
  }

  // ---------- 星空 ----------
  Mulberry32 rng(3);
  for (int r = 0; r < kPad - 9; ++r) {
    for (int c = 0; c < kWidth; ++c) {
      bool byRocket =
          c >= kLeft - 2 &&
          c <= kLeft + kRocketW + 1 + (std::abs(r - kArmRow) < 4 ? 12 : 0) &&
          r >= kPad - kRocketH - 2;
      bool byTower = std::abs(c - kTower - 1) < 4 && r >= kPad - 24;
      float p = rng.Next();
      if (p < 0.026f && !byRocket && !byTower) {
        Star st;
        st.c = c;
        st.r = r;
        st.phase = rng.Next() * 6.283f;
        st.big = rng.Next() < 0.2f;
        stars_.push_back(st);
      }
    }
  }

  // ---------- 尾焰喷团 ----------
  int n = static_cast<int>(std::ceil(static_cast<float>(kLoop) * kRate));
  puffs_.reserve(n);
  for (int i = 0; i < n; ++i) {
    Puff p;
    p.jx = (rng.Next() - 0.5f) * 2.4f;
    p.side = rng.Next() < 0.5f ? -1.f : 1.f;
    p.fan = 14.f + rng.Next() * 12.f;
    p.rise = 0.5f + rng.Next() * 0.9f;
    p.life = 2.2f + rng.Next() * 1.4f;
    p.grow = 0.6f + rng.Next() * 0.6f;
    puffs_.push_back(p);
  }
}

// ---------- 写入一格 -------------------------------------------------------
void Rocket::Put(float x, float y, QChar ch, uint8_t col) {
  int c = static_cast<int>(std::round(x));
  int r = static_cast<int>(std::round(y));
  if (c >= 0 && c < kWidth && r >= 0 && r < kHeight) {
    int k = r * kWidth + c;
    chars_[k] = ch;
    colors_[k] = col;
  }
}

// ---------- 往雾里抹一团 ---------------------------------------------------
void Rocket::Splat(float x, float y, float rad, float s) {
  float ry = rad / 2.f;
  int r_lo = std::max(0, static_cast<int>(std::ceil(y - ry)));
  int r_hi = std::min(kHeight - 1, static_cast<int>(std::floor(y + ry)));
  int c_lo = std::max(0, static_cast<int>(std::ceil(x - rad)));
  int c_hi = std::min(kWidth - 1, static_cast<int>(std::floor(x + rad)));
  for (int r = r_lo; r <= r_hi; ++r) {
    for (int c = c_lo; c <= c_hi; ++c) {
      float dx = (static_cast<float>(c) - x) / rad;
      float dy = (static_cast<float>(r) - y) / ry;
      float q = dx * dx + dy * dy;
      if (q < 1.f) {
        fog_[r * kWidth + c] += s * (1.f - q);
      }
    }
  }
}

// ---------- 一团尾焰从喷管喷出到散开 ---------------------------------------
void Rocket::PuffDraw(const Puff& p, float x0, float y0, float age,
                      float thick) {
  float down = 26.f * 0.2f * (1.f - std::exp(-age / 0.2f));
  float x = x0 + p.jx * (1.f + age);
  float y = y0 + down;
  if (y > static_cast<float>(kPad)) {
    float ratio = (static_cast<float>(kPad) - y0) / (26.f * 0.2f);
    float hit = -0.2f * std::log(std::max(1e-6f, 1.f - ratio));
    y = static_cast<float>(kPad) - 0.2f;
    x = x0 + p.jx +
        p.side * p.fan * 0.7f * (1.f - std::exp(-(age - hit) / 0.7f));
  }
  y -= p.rise * age * age * 0.5f + 0.3f * age;
  float fade = std::max(0.f, 1.f - age / p.life);
  Splat(x + 0.8f * age, y, 1.2f + p.grow * age * 2.2f,
        0.9f * thick * fade * fade);
}

// ---------- 每帧 -----------------------------------------------------------
void Rocket::Update(double time) {
  const float tf = static_cast<float>(time);
  float tt = std::fmod(tf, static_cast<float>(kLoop));
  if (tt < 0.f)
    tt += static_cast<float>(kLoop);

  std::fill(chars_.begin(), chars_.end(), QChar(' '));
  std::fill(colors_.begin(), colors_.end(), uint8_t(0));
  std::fill(fog_.begin(), fog_.end(), 0.f);

  // ---------- 星星 ----------
  for (const auto& s : stars_) {
    float tw = std::sin(tf * 1.7f + s.phase);
    QChar ch;
    if (s.big) {
      ch = (tw > 0.3f) ? QChar('+') : QChar('*');
    } else {
      ch = (tw > -0.6f) ? QChar('.') : QChar(' ');
    }
    Put(static_cast<float>(s.c), static_cast<float>(s.r), ch, 0);
  }

  // ---------- 塔架 ----------
  for (int r = kPad - 20; r <= kPad; ++r) {
    Put(static_cast<float>(kTower), static_cast<float>(r), QChar('|'), 0);
    Put(static_cast<float>(kTower + 2), static_cast<float>(r), QChar('|'), 0);
    Put(static_cast<float>(kTower + 1), static_cast<float>(r),
        QChar((r & 1) ? '/' : '\\'), 0);
  }
  Put(static_cast<float>(kTower + 1), static_cast<float>(kPad - 21), QChar('|'),
      0);
  {
    bool on = (static_cast<int>(std::floor(tf * 1.25)) % 2) == 1;
    Put(static_cast<float>(kTower + 1), static_cast<float>(kPad - 22),
        on ? QChar('*') : QChar('.'), 0);
  }

  // ---------- 服务臂 ----------
  float swing;
  if (tt < kArmT) {
    swing = 1.f;
  } else if (tt < kIgniteT) {
    swing = 1.f - (tt - kArmT) / (kIgniteT - kArmT);
  } else if (tt < kLandT + 1.f) {
    swing = 0.f;
  } else {
    swing = std::min(1.f, (tt - kLandT - 1.f) / 0.6f);
  }
  int arm_len = static_cast<int>(
      std::round(swing * static_cast<float>(kLeft + 1 - kTower)));
  for (int i = 0; i < arm_len; ++i) {
    Put(static_cast<float>(kTower + 3 + i), static_cast<float>(kArmRow),
        QChar('='), 0);
  }

  // ---------- 甲板与导流槽 ----------
  for (int c = 0; c < kWidth; ++c) {
    Put(static_cast<float>(c), static_cast<float>(kHeight - 1), QChar(0x2580),
        0);  // ▀
  }
  for (int c = 2; c < kWidth - 2; ++c) {
    bool in_trench = std::abs(static_cast<float>(c) - kMid) < 4.f;
    if (!in_trench) {
      Put(static_cast<float>(c), static_cast<float>(kPad), QChar('_'), 0);
    }
  }

  // ---------- 尾焰喷团 → 雾 ----------
  const int nPuffs = static_cast<int>(puffs_.size());
  for (int k = 0; k < nPuffs; ++k) {
    float born = static_cast<float>(k) / kRate;
    float age = tt - born;
    if (age < 0.f || age > puffs_[k].life)
      continue;
    if (!Burning(born))
      continue;
    float h = HeightAt(born);
    if (kPad - 1 - h < -2.f)
      continue;
    float thick = std::exp(-h / (born > kLandT - kBurn ? 4.f : 9.f));
    PuffDraw(puffs_[k], kMid, static_cast<float>(kPad) - 0.5f - h, age, thick);
  }

  // 从雾写出烟：SMOKE 斜坡 " .:-=+*#%"
  static const QChar kSmokeRamp[8] = {
      QChar('.'), QChar(':'), QChar('-'), QChar('='),
      QChar('+'), QChar('*'), QChar('#'), QChar('%'),
  };
  for (int r = 0; r < kHeight; ++r) {
    for (int c = 0; c < kWidth; ++c) {
      int k = r * kWidth + c;
      if (fog_[k] <= 0.06f)
        continue;
      float v = 1.f - std::exp(-fog_[k] / 0.9f);
      int idx = std::min(7, static_cast<int>(std::floor(v * 8.f)));
      Put(static_cast<float>(c), static_cast<float>(r), kSmokeRamp[idx], 0);
    }
  }

  // ---------- 尾翼支撑 ----------
  for (int r = kPad - 3; r < kPad; ++r) {
    Put(static_cast<float>(kLeft), static_cast<float>(r), QChar('['), 0);
    Put(static_cast<float>(kLeft + kRocketW - 1), static_cast<float>(r),
        QChar(']'), 0);
  }

  // ---------- 火箭 ----------
  float h = HeightAt(tt);
  if (h >= 0.f) {
    int base = kPad - 1 - static_cast<int>(std::round(h));
    for (int r = 0; r < kRocketH; ++r) {
      for (int c = span_lo_[r]; c < span_hi_[r]; ++c) {
        int rk = r * kRocketW + c;
        if (rkt_char_[rk] == QChar(' '))
          continue;
        Put(static_cast<float>(kLeft + c),
            static_cast<float>(base - kRocketH + 1 + r), rkt_char_[rk],
            rkt_color_[rk]);
      }
    }

    // boil-off：液氧从上上级排气阀缓缓漏出，随风格飘散
    for (int k = 23; k >= 0; --k) {
      float born = (std::floor(tf / 0.1f) - static_cast<float>(k)) * 0.1f;
      float age = tf - born;
      if (age > 2.4f)
        continue;
      float born_mod = std::fmod(born, static_cast<float>(kLoop));
      if (born_mod < 0.f)
        born_mod += static_cast<float>(kLoop);
      if (!OnPad(born_mod))
        continue;
      float f = age / 2.4f;
      float gust = 1.f + 0.3f * std::sin(0.9f * born);
      float y = static_cast<float>(base - kRocketH + 9) - 0.4f * age -
                0.2f * age * age + 1.1f * std::sin(2.3f * born) * f;
      float x = static_cast<float>(kLeft + kRocketW - 4) +
                gust * (3.f * age + 0.6f * age * age);
      QChar ch;
      if (f < 0.2f)
        ch = QChar('~');
      else if (f < 0.45f)
        ch = QChar('-');
      else if (f < 0.75f)
        ch = QChar('.');
      else
        ch = QChar(0x00b7);  // ·
      Put(x, y, ch, 0);
    }

    // ---------- 火焰 ----------
    if (Burning(tt)) {
      float len;
      if (tt < kLiftT) {
        len = 1.f + 2.5f * ((tt - kIgniteT) / (kLiftT - kIgniteT));
      } else if (tt < kLandT - kBurn) {
        len = 6.f;
      } else {
        len = 4.5f;
      }
      int f = static_cast<int>(std::floor(tf * 20.f));
      int extra = ((f * 7) % 3 + 3) % 3;
      float len_max = len + static_cast<float>(extra) - 1.f;

      static const QChar kFlameCh[4] = {
          QChar(0x2591),  // ░
          QChar(0x2592),  // ▒
          QChar(0x2593),  // ▓
          QChar(0x2588),  // █
      };

      for (int k = 1; k <= len_max; ++k) {
        float heat = 1.f - static_cast<float>(k - 1) / (len + 1.f);
        float half = 0.4f + 2.8f * std::pow(heat, 0.6f);
        int c_lo = static_cast<int>(std::ceil(kMid - half));
        int c_hi = static_cast<int>(std::floor(kMid + half));
        for (int c = c_lo; c <= c_hi; ++c) {
          float edge = std::abs(static_cast<float>(c) - kMid) / (half + 0.5f);
          int hash5 = ((f + c * 3 + k) * 13 % 5 + 5) % 5;
          float idxf = (heat * 0.8f + 0.35f - edge * 0.5f +
                        static_cast<float>(hash5) * 0.03f) *
                       4.f;
          int i = static_cast<int>(std::floor(idxf));
          if (i >= 0) {
            i = std::min(i, 3);
            Put(static_cast<float>(c), static_cast<float>(base + k),
                kFlameCh[i], 1);
          }
        }
      }
    }
  }
}