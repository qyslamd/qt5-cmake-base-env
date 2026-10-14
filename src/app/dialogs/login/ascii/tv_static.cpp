#include "tv_static.h"

#include <algorithm>
#include <cmath>

#include <QString>

// ============================== 常量 =======================================
static constexpr int kCols = TvStatic::kWidth;
static constexpr int kRows = TvStatic::kHeight;
static constexpr int kN = 7;  // RAMP 顶层索引（长度 8，减一）

// RAMP = " .:+░▒▓█"
static const QChar kRamp[8] = {
    QChar(0x0020), QChar(0x002E), QChar(0x003A), QChar(0x002B),
    QChar(0x2591), QChar(0x2592), QChar(0x2593), QChar(0x2588),
};

static constexpr float kLoop = 10.0f;  // 秒：雪花 → 调谐 → 测试卡 → 丢失 → 雪花
static constexpr float kFps = 20.0f;

static const int kBars[7] = {7, 6, 5, 4, 3, 2, 1};    // 彩条，亮到暗
static const int kCastle[7] = {1, 0, 3, 0, 5, 0, 7};  // 彩条下反色条

struct BottomEntry {
  float end;
  int level;
};
static const BottomEntry kBottom[8] = {
    {0.17f, 5}, {0.34f, 7}, {0.51f, 3}, {0.68f, 0},
    {0.73f, 1}, {0.78f, 0}, {0.83f, 2}, {1.00f, 0},
};
static constexpr int kBottomN = 8;

// 雪花逐级抬升的阈值
static const float kSnow[7] = {0.36f, 0.48f, 0.60f, 0.70f, 0.80f, 0.90f, 0.98f};

static const char* const kPaletteHex[TvStatic::kPaletteN] = {
    "#d8dcd0",  // 冷白，略偏绿，像老显像管的荧光粉
};

// ============================== 哈希与平滑 =================================
static inline uint32_t Mul32(uint32_t a, uint32_t b) { return a * b; }

static float HashT(int a, int b, int c) {
  uint32_t h = Mul32(static_cast<uint32_t>(a), 0x27d4eb2du) ^
               Mul32(static_cast<uint32_t>(b), 0x165667b1u) ^
               Mul32(static_cast<uint32_t>(c), 0x2545f491u);
  h = Mul32(h ^ (h >> 15), 0x85ebca6bu);
  h = Mul32(h ^ (h >> 13), 0xc2b2ae35u);
  h = h ^ (h >> 16);
  return static_cast<float>(h) / 4294967296.0f;
}

static inline float SmoothTS(float a, float b, float v) {
  float x = (v - a) / (b - a);
  if (x < 0.0f) {
    x = 0.0f;
  }
  if (x > 1.0f) {
    x = 1.0f;
  }
  return x * x * (3.0f - 2.0f * x);
}

static inline QChar InkChar(int level) {
  if (level < 0) {
    level = 0;
  }
  if (level > kN) {
    level = kN;
  }
  return kRamp[level];
}

// ============================== 公开接口 ===================================
const char* const* TvStatic::PaletteHex() { return kPaletteHex; }

TvStatic::TvStatic() { Init(); }

// ============================== 静态构建 ===================================
void TvStatic::Init() {
  frame_.assign(kCols * kRows, QChar(' '));
  DrawSetFrame();

  card_lv_.assign(kSW * kSH, 0);
  card_gl_.assign(kSW * kSH, QChar(' '));
  DrawTestCard();

  chars_.assign(kCols * kRows, QChar(' '));
  colors_.assign(kCols * kRows, 0);
}

// ---------- 电视机柜：外壳、圆角屏框、两个旋钮、喇叭栅、机脚、天线 ---------
void TvStatic::DrawSetFrame() {
  const int cols = kCols;

  auto put = [&](int r, int c, const char* utf8) {
    const QString q = QString::fromUtf8(utf8);
    for (int i = 0; i < q.size(); ++i) {
      const int cc = c + i;
      if (cc >= 0 && cc < cols) {
        frame_[r * cols + cc] = q[i];
      }
    }
  };

  // c[0..3] = 四角 / c[4] = 横线 / c[5] = 竖线
  auto box = [&](int x0, int y0, int x1, int y1, const char* utf8) {
    const QString c = QString::fromUtf8(utf8);
    for (int x = x0 + 1; x < x1; ++x) {
      frame_[y0 * cols + x] = c[4];
      frame_[y1 * cols + x] = c[4];
    }
    for (int y = y0 + 1; y < y1; ++y) {
      frame_[y * cols + x0] = c[5];
      frame_[y * cols + x1] = c[5];
    }
    frame_[y0 * cols + x0] = c[0];
    frame_[y0 * cols + x1] = c[1];
    frame_[y1 * cols + x0] = c[2];
    frame_[y1 * cols + x1] = c[3];
  };

  box(1, 5, 56, 23, "╭╮╰╯─│");    // 外壳
  box(3, 6, 43, 22, "╭╮╰╯─│");    // 圆角屏框
  box(46, 8, 52, 10, "╭╮╰╯─│");   // 频道旋钮
  box(46, 12, 52, 14, "╭╮╰╯─│");  // 音量旋钮
  box(45, 16, 53, 21, "┌┐└┘─│");  // 喇叭

  for (int y = 17; y <= 20; ++y) {
    for (int x = 46; x <= 52; ++x) {
      frame_[y * cols + x] = QChar(0x2550);  // ═
    }
  }

  frame_[13 * cols + 49] = QChar(0x2572);  // ╲ 音量旋钮上的指针

  put(4, 25, "▄▄███▄▄");  // 机顶的散热条

  for (int kk = 1; kk <= 4; ++kk) {
    frame_[(4 - kk) * cols + (25 - kk)] = QChar(0x2572);  // ╲
    frame_[(4 - kk) * cols + (31 + kk)] = QChar(0x2571);  // ╱
  }
  frame_[0 * cols + 21] = QChar('o');  // 天线帽
  frame_[0 * cols + 35] = QChar('o');

  put(24, 5, "╱");
  put(25, 4, "╱");
  put(24, 52, "╲");
  put(25, 53, "╲");
}

// ---------- 测试卡：彩条 / 反色条 / 底部硬边块，覆一个圆加十字准线 -------
void TvStatic::DrawTestCard() {
  const int SW = kSW, SH = kSH;

  // 上半彩条、中段城堡条、下半硬边块
  for (int r = 0; r < SH; ++r) {
    for (int c = 0; c < SW; ++c) {
      const float u = (c + 0.5f) / SW;
      const float v = (r + 0.5f) / SH;
      const int bar = std::min(6, static_cast<int>(std::floor(u * 7.0f)));
      int level;
      if (v < 0.67f) {
        level = kBars[bar];
      } else if (v < 0.75f) {
        level = kCastle[bar];
      } else {
        level = 0;
        for (int i = 0; i < kBottomN; ++i) {
          if (u < kBottom[i].end) {
            level = kBottom[i].level;
            break;
          }
        }
      }
      card_lv_[r * SW + c] = static_cast<int8_t>(level);
    }
  }

  // 圆的每行跨度，用带角标的横线描出
  const float R = std::min(SH * 0.43f, SW * 0.22f) * 2.0f;
  const float cx = SW / 2.0f, cy = SH / 2.0f;

  std::vector<int> spanA(SH, -1), spanB(SH, -1);
  for (int r = 0; r < SH; ++r) {
    const float y = (r + 0.5f - cy) * 2.0f;
    const float w = R * R - y * y;
    if (w >= 0.0f) {
      spanA[r] = static_cast<int>(std::ceil(cx - std::sqrt(w) - 0.5f));
      spanB[r] = static_cast<int>(std::floor(cx + std::sqrt(w) - 0.5f));
    }
  }

  const int mid = SH / 2;
  const int mc = SW / 2;

  auto setGlyph = [&](int r, int c, QChar g) {
    if (r < 0 || r >= SH || c < 0 || c >= SW) {
      return;
    }
    const int i = r * SW + c;
    card_lv_[i] = -1;
    card_gl_[i] = g;
  };

  for (int r = 0; r < SH; ++r) {
    if (spanA[r] < 0) {
      continue;
    }
    const int a = spanA[r], b = spanB[r];
    const bool up = r < mid;

    // 相邻行（靠近中缝的那一侧）的跨度
    int na, nb;
    const int nr = up ? r - 1 : r + 1;
    const bool hasNear = (nr >= 0 && nr < SH && spanA[nr] >= 0);
    if (hasNear) {
      na = spanA[nr];
      nb = spanB[nr];
    } else {
      na = mc + 1;
      nb = mc - 1;
    }

    for (int c = a; c <= b; ++c) {
      if (c > a && c < na && c < nb) {
        setGlyph(r, c, QChar(0x2500));  // ─
      } else if (c < b && c > nb && c > na) {
        setGlyph(r, c, QChar(0x2500));
      }
    }
    if (a < na) {
      setGlyph(r, a, up ? QChar(0x256D) : QChar(0x2570));   // ╭ / ╰
      setGlyph(r, na, up ? QChar(0x256F) : QChar(0x256E));  // ╯ / ╮
      setGlyph(r, b, up ? QChar(0x256E) : QChar(0x256F));   // ╮ / ╯
      setGlyph(r, nb, up ? QChar(0x2570) : QChar(0x256D));  // ╰ / ╭
    } else {
      setGlyph(r, a, QChar(0x2502));  // │
      setGlyph(r, b, QChar(0x2502));
    }
    if (!hasNear) {
      for (int c = a + 1; c < b; ++c) {
        setGlyph(r, c, QChar(0x2500));
      }
    }
  }

  // 十字准线，与圆相交处用 T 型接头
  if (spanA[mid] >= 0) {
    const int a = spanA[mid], b = spanB[mid];
    for (int c = a + 1; c < b; ++c) {
      setGlyph(mid, c, QChar(0x2500));  // ─
    }
    for (int r = 0; r < SH; ++r) {
      if (spanA[r] >= 0) {
        setGlyph(r, mc, QChar(0x2502));  // │
      }
    }

    setGlyph(mid, a, QChar(0x251C));   // ├
    setGlyph(mid, b, QChar(0x2524));   // ┤
    setGlyph(mid, mc, QChar(0x253C));  // ┼

    int first = -1, last = -1;
    for (int r = 0; r < SH; ++r) {
      if (spanA[r] >= 0) {
        if (first < 0) {
          first = r;
        }
        last = r;
      }
    }
    if (first >= 0) {
      setGlyph(first, mc, QChar(0x252C));  // ┬
    }
    if (last >= 0) {
      setGlyph(last, mc, QChar(0x2534));  // ┴
    }
  }
}

// ---------- 画面区：用超椭圆裁掉圆角显像管外的部分 -----------------------
bool TvStatic::Inside(int c, int r) const {
  const float x = (c + 0.5f - kSW / 2.0f) / (kSW / 2.0f);
  const float y = (r + 0.5f - kSH / 2.0f) / (kSH / 2.0f);
  const float x6 = x * x * x * x * x * x;
  const float y6 = y * y * y * y * y * y;
  return x6 + y6 <= 1.02f;
}

// ---------- 每帧 -----------------------------------------------------------
void TvStatic::Update(double time) {
  const float t = static_cast<float>(time);
  const int cols = kCols;

  // 循环时钟
  float u = std::fmod(t, kLoop);
  if (u < 0.0f) {
    u += kLoop;
  }

  // 雪花每帧都换新，所以按帧号取
  const int k = static_cast<int>(std::floor(t * kFps));

  // 信号抓得多牢：临近锁定、快要丢失时，旋钮咔哒过三档
  const float lock = SmoothTS(1.6f, 3.4f, u) * (1.0f - SmoothTS(6.6f, 7.8f, u));
  const int dialRaw1 = static_cast<int>(std::floor((u - 1.1f) * 8.0f));
  const int dialRaw2 = 3 - static_cast<int>(std::floor((u - 6.4f) * 8.0f));
  int dial = std::min(3, std::min(dialRaw1, dialRaw2));
  if (dial < 0) {
    dial = 0;
  }

  const float snow = 1.0f - 0.97f * lock;

  // 未锁定时画面滚动（带着消隐条），扫描线撕裂
  const float roll = (1.0f - lock) * (1.0f - lock) * (kSH + 2) * 2.5f;
  const float tear = (1.0f - lock) * 9.0f;

  // 交流声条的中心行，一圈走一遍
  float hum = std::fmod(u / kLoop + 0.45f, 1.0f);
  if (hum < 0.0f) {
    hum += 1.0f;
  }
  hum = hum * (kSH + 6) - 3.0f;

  // 从电视机柜起手
  chars_ = frame_;

  // 频道旋钮指针随拨档换形
  static const QChar kDial[4] = {
      QChar(0x2571),  // ╱
      QChar(0x2500),  // ─
      QChar(0x2572),  // ╲
      QChar(0x2502),  // │
  };
  chars_[9 * cols + 49] = kDial[dial];

  // 扫过画面区
  for (int r = 0; r < kSH; ++r) {
    // 交流声：以 hum 为中心的高斯暗带
    const float dr = static_cast<float>(r) - hum;
    const float dim = 1.0f - 0.45f * std::exp(-(dr * dr) / (2.2f * 2.2f));
    // 每条扫描线或亮或暗一点
    const float line = 0.9f + 0.2f * HashT(k, r, 9);
    // 撕裂：按行与时间做横向抖动
    const float shiftF =
        tear * std::sin(r * 0.8f + u * 9.0f) * HashT(k >> 2, r, 3);
    const int shift = static_cast<int>(std::round(shiftF));
    // 这张卡此刻被滚到了哪一行
    float prF =
        std::fmod(static_cast<float>(r) + roll, static_cast<float>(kSH + 2));
    if (prF < 0.0f) {
      prF += (kSH + 2);
    }
    const int pr = static_cast<int>(std::floor(prF));

    // 行内一阶低通，让雪花沿扫描线拉出细丝
    float prev = HashT(k, r, 1);

    for (int c = 0; c < kSW; ++c) {
      const int outIdx = (kY0 + r) * cols + (kX0 + c);
      if (!Inside(c, r)) {
        chars_[outIdx] = QChar(' ');
        continue;
      }

      const int h1 = r * 97 + c;
      const float n = HashT(k, h1, 2);
      prev = 0.6f * n + 0.4f * prev;

      const bool noisy = HashT(k, h1, 5) < snow;

      QChar ch;
      if (noisy && HashT(k, h1, 6) >= std::pow(lock, 6.0f)) {
        // 纯雪花
        const float v = prev * line * dim;
        int level = 0;
        while (level < 7 && v > kSnow[level]) {
          ++level;
        }
        ch = InkChar(level);
      } else {
        // 测色卡（锁定后仍有一点残留噪声在抖）
        int pc = c + shift;
        pc = ((pc % kSW) + kSW) % kSW;
        if (pr < kSH) {
          const int ci = pr * kSW + pc;
          if (card_lv_[ci] >= 0) {
            int lv = card_lv_[ci];
            if (noisy) {
              lv += (n < 0.5f) ? -1 : 1;
              if (lv < 0) {
                lv = 0;
              }
              if (lv > kN) {
                lv = kN;
              }
            }
            ch = InkChar(lv);
          } else {
            ch = card_gl_[ci];
          }
        } else {
          // 滚动的消隐条：整行空
          ch = InkChar(0);
        }
      }
      chars_[outIdx] = ch;
    }
  }
}