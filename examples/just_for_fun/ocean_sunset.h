// ============================================================================
//  ocean sunset — 海岬之后的黄金时刻，逐格半色调终端艺术
//  原作: github.com/bas3line/ascii (TypeScript, MIT)
//  Qt 5.15.2 C++ 直译
// ============================================================================
#ifndef EXAMPLES_JUST_FOR_FUN_OCEAN_SUNSET_H_
#define EXAMPLES_JUST_FOR_FUN_OCEAN_SUNSET_H_

#include <array>
#include <cstdint>
#include <vector>

#include "halftone_common.h"

class OceanSunset {
 public:
  static constexpr int kWidth = 200;
  static constexpr int kHeight = 100;
  static constexpr int kPaletteN = 43;

  OceanSunset();
  void Update(double t);

  const std::vector<uint8_t>& stepBuf() const { return step_; }
  const std::vector<uint8_t>& colorBuf() const { return color_; }

  static const char* const* PaletteHex();

 private:
  // ---- 调色板 ----------------------------------------------------------
  HalftonePalette palette_;

  // ---- 静态尺寸 --------------------------------------------------------
  static constexpr int kHorizon = 56;
  static constexpr int kLandW = 60;
  static constexpr int kLandH = kHorizon + 10;  // 66
  static constexpr int kDeckU = 640;
  static constexpr int kDeckV = 128;
  static constexpr int kLowerW = 720;

  // ---- 静态预计算（一次性） -------------------------------------------
  std::vector<float> sky_r_, sky_g_, sky_b_;     // kHorizon*kWidth
  std::vector<uint8_t> land_;                    // N  0 无 / 1 岩 / 2 松 / 3 船
  std::vector<float> land_r_, land_g_, land_b_;  // N
  std::vector<float> deck_, deck_lit_;           // kDeckU*kDeckV
  std::vector<float> thin_top_;                  // kWidth*kHorizon
  std::vector<float> lo_cover_, lo_lit_;         // kLowerW*kHorizon

  // ---- 每帧缓冲 --------------------------------------------------------
  std::vector<float> sk_r_, sk_g_, sk_b_;  // kHorizon*kWidth  本帧天空
  std::vector<uint8_t> step_, color_;      // N

  void Init();
  void Halftone(int k, int r, int x, float cr, float cg, float cb,
                float base_level, float fade);
};

#endif  // EXAMPLES_JUST_FOR_FUN_OCEAN_SUNSET_H_