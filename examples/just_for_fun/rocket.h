// ============================================================================
//  rocket — 夜间发射：boil-off 排气、服务臂后摆、发动机点火、
//           尾焰从导流槽喷出、火箭升空出画、再以着陆点火降回
//  原作: github.com/bas3line/ascii (TypeScript, MIT)
//  Qt 5.15.2 C++ 直译
// ============================================================================
#ifndef EXAMPLES_JUST_FOR_FUN_ROCKET_H_
#define EXAMPLES_JUST_FOR_FUN_ROCKET_H_

#include <array>
#include <cstdint>
#include <vector>

class Rocket {
 public:
  static constexpr int kWidth = 48;
  static constexpr int kHeight = 28;
  static constexpr int kPaletteN = 6;

  Rocket();
  void Update(double t);

  const std::vector<uint8_t>& stepBuf() const { return step_; }
  const std::vector<uint8_t>& colorBuf() const { return color_; }

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

  // 预计算
  std::vector<Star> stars_;
  std::vector<Puff> puffs_;

  // 火箭精灵：每格一个 (step, color)，预先一次算好
  std::vector<uint8_t> rkt_step_, rkt_color_;
  std::array<int, kRocketH> span_lo_{}, span_hi_{};

  // 每帧输出
  std::vector<uint8_t> step_, color_;
  std::vector<float> fog_;

  void Init();
  void Put(float x, float y, uint8_t s, uint8_t col);
  void Splat(float x, float y, float rad, float s);
  void PuffDraw(const Puff& p, float x0, float y0, float age, float thick);
};

#endif  // EXAMPLES_JUST_FOR_FUN_ROCKET_H_