// ============================================================================
//  rocket — 夜间发射：boil-off 排气、服务臂后摆、发动机点火、
//           尾焰从导流槽喷出、火箭升空出画、再以着陆点火降回
//  原作: github.com/bas3line/ascii (TypeScript, MIT)
//  Qt 5.15.2 C++ 直译
//
//  输出 (QChar, colorIdx) 字符缓冲，配合 TextView 用等宽字体画。
//  字符本身携带几何（/ \ | ░ ▒ ▓ █ 等），不压成圆点。
// ============================================================================
#ifndef FILE_APP_DIALOGS_ASCII_ROCKET_H_
#define FILE_APP_DIALOGS_ASCII_ROCKET_H_

#include <array>
#include <cstdint>
#include <vector>

#include <QChar>

class Rocket {
 public:
  static constexpr int kWidth = 48;
  static constexpr int kHeight = 28;
  static constexpr int kPaletteN = 2;

  Rocket();
  void Update(double t);

  const std::vector<QChar>& charBuf() const { return chars_; }
  const std::vector<uint8_t>& colorBuf() const { return colors_; }

  static const char* const* PaletteHex();

 private:
  static constexpr int kRocketW = 14;
  static constexpr int kRocketH = 21;

  struct Star {
    int c, r;
    float phase;
    bool big;
  };
  struct Puff {
    float jx, side, fan, rise, life, grow;
  };

  // 静态预计算
  std::vector<Star> stars_;
  std::vector<Puff> puffs_;

  // 火箭精灵：每格一个 (QChar, colorIdx)
  std::vector<QChar> rkt_char_;
  std::vector<uint8_t> rkt_color_;
  std::array<int, kRocketH> span_lo_{}, span_hi_{};

  // 每帧输出
  std::vector<QChar> chars_;
  std::vector<uint8_t> colors_;
  std::vector<float> fog_;

  void Init();
  void Put(float x, float y, QChar ch, uint8_t col = 0);
  void Splat(float x, float y, float rad, float s);
  void PuffDraw(const Puff& p, float x0, float y0, float age, float thick);
};

#endif  // FILE_APP_DIALOGS_ASCII_ROCKET_H_