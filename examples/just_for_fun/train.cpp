#include "train.h"

#include <algorithm>
#include <cmath>

// ============================== 常量 =======================================
static constexpr int kLen = 73;        // 列车长度：尾部缓冲器到排障器前端
static constexpr int kRail = 23;       // 铁轨行
static constexpr float kSpeed = 8.0f;  // 每秒推进多少列

static constexpr int kDrive[2] = {42, 52};  // 驱动轮中心列
static constexpr int kSmall[8] = {3, 6, 12, 15, 22, 25, 28, 31};  // 小轮列

static constexpr int kCross = 58;  // 主连杆和十字头交汇处，半行程
static constexpr int kCyl = 60;    // 气缸后端
static constexpr int kStack = 62;  // 烟囱列

static constexpr float kRim = 2.5f;   // 驱动轮半径（行）
static constexpr float kLife = 5.5f;  // 一股烟存活秒数
static constexpr float kWind = 2.4f;  // 每秒往回漂多少列

static constexpr int kSpan =
    Train::kWidth + kLen + 1;  // 一圈：从左边进、穿过去、出画
static constexpr int kX0 =
    kLen - 16;  // t=0 时机车和煤水车已经在画面内，车厢正要进

static constexpr float kPi = 3.14159265358979323846f;
static constexpr float kOmega = kSpeed / (2.0f * kRim);  // 车轮每秒转过的弧度
static constexpr float kBeat = kPi / 2.0f / kOmega;      // 每转四拍

// 烟从薄到厚
static const char kSmoke[] = " .:-=+*";
static constexpr int kSmokeN = static_cast<int>(sizeof(kSmoke)) - 1;  // 7

static const char* const kPaletteHex[Train::kPaletteN] = {
    "#d8d8e8",
};

// ============================== 哈希 =======================================
// Math.imul 语义（保留低 32 位，按无符号乘法）
static inline uint32_t Mul32(uint32_t a, uint32_t b) { return a * b; }

static float HashT(int x, int y) {
  uint32_t h = Mul32(static_cast<uint32_t>(x), 0x27d4eb2du) ^
               Mul32(static_cast<uint32_t>(y), 0x165667b1u);
  h = Mul32(h ^ (h >> 15), 0x85ebca6bu);
  h = h ^ (h >> 13);
  return static_cast<float>(h) / 4294967296.0f;
}

// clang-format off
// ============================== 精灵数据 ===================================
// 列车侧影：以 [顶部行, 左列, 逐行字符串] 拼成。空格是透明的。
// 注意：TS 源里用了 String.raw，所以 `\\` 表示两个字面反斜杠，
//       C++ 字符串里每个反斜杠写成 `\\`。

static const char* const kPart0[7] = {
    " ________________ ",
    "/________________\\ ",
    "| __  __  __  __ | ",
    "||  ||  ||  ||  || ",
    "||__||__||__||__|| ",
    "|________________|=",
    " |=|=|      |=|=|  ",
};

static const char* const kPart1[6] = {
    "  _.-^^^^-._  ",
    " |^^^^^^^^^^| ",
    " |          | ",
    " |          | ",
    " |__________|=",
    "  |=|=||=|=|  ",
};

static const char* const kPart2[14] = {
    "                           _____ ",
    "                           \\   / ",
    " _________      .-.  _      | |  ",
    " \\_______/_____/   \\| |_____| |__[]",
    "  | ___ |  ||      ||      ||    \\ ",
    "  ||   ||  ||      ||      ||     )",
    "  ||___||  ||      ||      ||     )",
    "  |     |__||______||______||____/ ",
    "  |_____|=========================[]",
    "  | |                             |\\ ",
    "  |_|                       ____  |\\\\ ",
    "                           |____| |\\\\\\ ",
    "                                  | \\\\\\ ",
    "                                  |__\\\\\\ ",
};

// 驱动轮：轮辋、辐条正对镜头、再转八分之一圈
static const char* const kWheel[5] = {
    "  .---.  ",
    " /     \\ ",
    "|  (o)  |",
    " \\     / ",
    "  '---'  ",
};

static const char* const kSpokes[2][3] = {
    {
        "    |    ",
        " --   -- ",
        "    |    ",
    },
    {
        "   \\ /   ",
        "         ",
        "   / \\   ",
    },
};

// clang-format on

// ============================== 公开接口 ===================================
const char* const* Train::PaletteHex() { return kPaletteHex; }

Train::Train() { Init(); }

// ============================== 静态构建 ===================================
void Train::Init() {
  const int N = kWidth * kHeight;
  chars_.assign(N, QChar(' '));
  colors_.assign(N, 0);
  dens_.assign(N, 0.0f);
}

// ---------- 单格写入 -------------------------------------------------------
void Train::Put(int r, int c, QChar ch) {
  if (r < 0 || r >= kHeight || c < 0 || c >= kWidth)
    return;
  chars_[r * kWidth + c] = ch;
}

// ---------- 写整行（跳过空格，与 TS 的 text() 一致） ----------------------
void Train::PutStr(int r, int c0, const char* s) {
  if (r < 0 || r >= kHeight)
    return;
  for (int i = 0; s[i] != '\0'; ++i) {
    if (s[i] == ' ')
      continue;
    const int c = c0 + i;
    if (c < 0 || c >= kWidth)
      continue;
    chars_[r * kWidth + c] = QChar(s[i]);
  }
}

// ---------- 每帧 -----------------------------------------------------------
void Train::Update(double time) {
  const float t = static_cast<float>(time);

  std::fill(chars_.begin(), chars_.end(), QChar(' '));
  std::fill(dens_.begin(), dens_.end(), 0.0f);

  // 列车左边缘列（可正可负：负表示整车还在画面左侧外）
  auto Left = [](float tt) -> int {
    const float p = std::fmod(static_cast<float>(kX0) + kSpeed * tt,
                              static_cast<float>(kSpan));
    const float q = (p < 0.0f) ? p + static_cast<float>(kSpan) : p;
    return static_cast<int>(std::floor(q)) - kLen;
  };

  const int ox = Left(t);

  // ---------- 烟：从烟囱放开，留在原地，边升边胀、边被风往回吹薄 --------
  const int kStart = static_cast<int>(std::ceil((t - kLife) / kBeat));
  for (int k = kStart; static_cast<float>(k) * kBeat <= t; ++k) {
    const float kt = static_cast<float>(k) * kBeat;
    const float age = t - kt;
    const float u = age / kLife;

    const float x = static_cast<float>(Left(kt)) + kStack - kWind * age +
                    0.8f * std::sin(static_cast<float>(k) * 1.7f);
    const float y = 8.4f - 7.2f * (1.0f - std::exp(-age / 2.0f)) +
                    0.5f * std::sin(static_cast<float>(k) * 2.9f);
    const float rad = 1.5f + 0.75f * std::pow(age, 0.8f);
    const float w = std::pow(1.0f - u, 1.2f);

    const int r_lo = std::max(0, static_cast<int>(std::floor(y - rad)));
    const int r_hi = std::min(kHeight - 1, static_cast<int>(y + rad));
    const int c_lo = std::max(0, static_cast<int>(std::floor(x - 2.0f * rad)));
    const int c_hi = std::min(kWidth - 1, static_cast<int>(x + 2.0f * rad));

    for (int r = r_lo; r <= r_hi; ++r) {
      for (int c = c_lo; c <= c_hi; ++c) {
        const float dx = (c + 0.5f - x) / (2.0f * rad);
        const float dy = (r + 0.5f - y) / rad;
        const float q = dx * dx + dy * dy;
        if (q >= 1.0f)
          continue;
        // 上方受光，每一团都保留它压在前一团上的圆顶
        const float v =
            w * std::sqrt(1.0f - q) * (0.6f - 0.4f * dy - 0.1f * dx);
        const int j = r * kWidth + c;
        if (v > dens_[j])
          dens_[j] = v;
      }
    }
  }
  for (int j = 0; j < kWidth * kHeight; ++j) {
    const float v = dens_[j];
    if (v > 0.08f) {
      const int idx =
          std::min(kSmokeN - 1, static_cast<int>(std::ceil(v * 7.0f)));
      chars_[j] = QChar(kSmoke[idx]);
    }
  }

  // ---------- 铁轨与道砟（静态） ---------------------------------------
  for (int c = 0; c < kWidth; ++c) {
    Put(kRail, c, QChar('='));
    Put(kRail + 1, c, QChar((c % 3) ? '_' : '|'));
    const float h = HashT(c, 5);
    const char ballast = h < 0.16f   ? ':'
                         : h < 0.34f ? ','
                         : h < 0.44f ? '\''
                         : h < 0.70f ? '.'
                                     : ' ';
    Put(kRail + 2, c, QChar(ballast));
  }

  // ---------- 车体：三块拼装 ---------------------------------------------
  for (int i = 0; i < 7; ++i)
    PutStr(14 + i, ox + 0, kPart0[i]);
  for (int i = 0; i < 6; ++i)
    PutStr(15 + i, ox + 19, kPart1[i]);
  for (int i = 0; i < 14; ++i)
    PutStr(9 + i, ox + 33, kPart2[i]);

  // ---------- 驱动轮与辐条 -------------------------------------------------
  const float a = kOmega * t;
  const int eighth = static_cast<int>(std::floor(a / (kPi / 4.0f))) % 2;
  for (int di = 0; di < 2; ++di) {
    const int c = kDrive[di];
    for (int i = 0; i < 5; ++i)
      PutStr(kRail - 5 + i, ox + c - 4, kWheel[i]);
    for (int i = 0; i < 3; ++i)
      PutStr(kRail - 4 + i, ox + c - 4, kSpokes[eighth][i]);
  }

  // ---------- 小轮：随滚动换形 ---------------------------------------------
  const int roll = static_cast<int>(std::floor(kSpeed * t / 1.4f)) % 2;
  for (int si = 0; si < 8; ++si) {
    const int c = kSmall[si];
    PutStr(kRail - 2, ox + c - 1, ".-.");
    PutStr(kRail - 1, ox + c - 1, roll ? "(+)" : "(x)");
  }

  // ---------- 连杆：两个曲柄销随轮一起转，主连杆伸到十字头 ----------------
  // 连杆从轮辋和轮毂后面穿过。
  const int hub = kRail - 3;
  const int dr = -static_cast<int>(std::round(1.2f * std::sin(a)));
  const int dc =
      static_cast<int>(std::round((dr != 0 ? 2.4f : 3.0f) * std::cos(a)));
  const int pr = hub + dr;

  auto Free = [&](int r, int c) -> bool {
    for (int di = 0; di < 2; ++di) {
      const int d = kDrive[di];
      const int thresh = (r == hub) ? 4 : 3;
      if (std::abs(c - d) == thresh)
        return false;
      if (r == hub && std::abs(c - d) <= 1)
        return false;
    }
    return true;
  };

  // 两轮之间的连杆
  for (int c = kDrive[0] + dc; c <= kDrive[1] + dc; ++c) {
    if (!Free(pr, c))
      continue;
    const bool pin = (c == kDrive[0] + dc) || (c == kDrive[1] + dc);
    Put(pr, ox + c, QChar(pin ? 'o' : '='));
  }

  // 主连杆：从后销到十字头
  const int x0 = kDrive[1] + dc;
  const int xh = kCross + static_cast<int>(std::round(std::cos(a)));
  for (int c = x0 + 1; c < xh; ++c) {
    const int r = static_cast<int>(
        std::round(pr + (static_cast<float>(hub - pr) * (c - x0)) /
                            static_cast<float>(xh - x0)));
    if (!Free(r, c))
      continue;
    Put(r, ox + c, QChar(r == pr ? '=' : '-'));
  }
  Put(hub, ox + xh, QChar('#'));
  for (int c = xh + 1; c < kCyl; ++c)
    Put(hub, ox + c, QChar('='));
}