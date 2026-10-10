// ============================================================================
//  desert night — 无月沙丘上的银河与孤树，逐格半色调终端艺术
//  原作: github.com/bas3line/ascii (TypeScript, MIT)
//  Qt 5.15.2 C++ 直译
// ============================================================================
#ifndef EXAMPLES_JUST_FOR_FUN_DESERT_NIGHT_H_
#define EXAMPLES_JUST_FOR_FUN_DESERT_NIGHT_H_

#include <array>
#include <cstdint>
#include <vector>

#include "halftone_common.h"

class DesertNight {
 public:
  static constexpr int kWidth = 200;
  static constexpr int kHeight = 100;
  static constexpr int kPaletteN = 43;

  DesertNight();
  void Update(double t);

  const std::vector<uint8_t>& stepBuf() const { return step_; }
  const std::vector<uint8_t>& colorBuf() const { return color_; }

  static const char* const* PaletteHex();

 private:
  // ---- 调色板 ----------------------------------------------------------
  HalftonePalette palette_;

  // ---- 静态缓冲（一次性） ---------------------------------------------
  std::vector<uint8_t> mat_;  // 0 天空 / 1 沙丘 / 2 树
  std::vector<float> depth_;  // 每格命中距离
  std::vector<float> sand_r_, sand_g_, sand_b_;
  std::vector<float> crest_;         // 沙脊闪烁强度
  std::vector<float> bark_;          // 树冠上缘的暖光
  std::array<int16_t, kWidth> top_;  // 每列最顶部的沙丘行

  // ---- 天空（静态） ----------------------------------------------------
  std::vector<float> sky_r_, sky_g_, sky_b_;
  std::vector<float> air_;   // 气辉
  std::vector<float> band_;  // 银河带强度

  // ---- 每格抖动 --------------------------------------------------------
  std::vector<float> dith_;    // 天空：半有序抖动；沙丘/树：Bayer
  std::vector<float> floor0_;  // 各材质的最低覆盖

  // ---- 星星 ------------------------------------------------------------
  struct Star {
    int k;
    float bright;
    float tr, tg, tb;
    float rate, phase;
  };
  std::vector<Star> stars_;

  // ---- 小镇灯光 --------------------------------------------------------
  struct Lamp {
    int k;
    float phase, rate;
  };
  std::vector<Lamp> lamps_;

  // ---- 每帧缓冲 --------------------------------------------------------
  std::vector<float> fr_, fg_, fb_;
  std::vector<float> floor_;

  // ---- 输出 ------------------------------------------------------------
  std::vector<uint8_t> step_, color_;

  void Init();
};

#endif  // EXAMPLES_JUST_FOR_FUN_DESERT_NIGHT_H_