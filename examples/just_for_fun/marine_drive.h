// ============================================================================
//  marine drive — Mumbai 皇后项链夜景，逐格半色调终端艺术
//  原作: github.com/bas3line/ascii (TypeScript, MIT)
//  Qt 5.15.2 C++ 直译
// ============================================================================
#ifndef EXAMPLES_JUST_FOR_FUN_MARINE_DRIVE_H_
#define EXAMPLES_JUST_FOR_FUN_MARINE_DRIVE_H_

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "halftone_common.h"

class MarineDrive {
 public:
  static constexpr int kWidth = 200;
  static constexpr int kHeight = 100;
  static constexpr int kPaletteN = 36;

  MarineDrive();
  void Update(double t);

  const std::vector<uint8_t>& stepBuf() const { return step_; }
  const std::vector<uint8_t>& colorBuf() const { return color_; }

  static const char* const* PaletteHex();

 private:
  // ---- 静态缓冲 --------------------------------------------------------
  std::vector<uint8_t> mat_, lane_;
  std::vector<float> s_r_, s_g_, s_b_, flo_, sky_glow_;
  std::vector<float> lamp_i_, lamp_w_, flick_i_, flick_w_;
  std::array<std::vector<float>, 3> refl_;
  std::vector<float> refl_fl_;
  std::vector<float> cover_, clit_, clear_;

  // ---- 调色板 ----------------------------------------------------------
  HalftonePalette palette_;

  // ---- 弧长参数化 ------------------------------------------------------
  static constexpr float kStepU = 0.25f;
  static constexpr int kCurveSamples = 722;
  std::vector<float> u_tab_;
  float u_end_ = 0;
  float Uof(float x) const;
  float XOfU(float u) const;

  // ---- 建筑 ------------------------------------------------------------
  struct Building {
    float u0, u1;
    int floors;
    float tone;
    bool crown;
  };
  std::vector<Building> blds_;
  std::array<float, kWidth> col_u_{};
  std::array<int16_t, kWidth> bld_at_{};

  // ---- 路灯 ------------------------------------------------------------
  struct Lamp {
    float x, y, s;
  };
  std::vector<Lamp> lamps_;

  // ---- 闪烁 / 信标 -----------------------------------------------------
  struct Flick {
    int k;
    float wr, wg, wb;
    int kind;
    float rate, phase;
  };
  std::vector<Flick> flick_;
  std::vector<std::pair<int, float>> beacons_;

  // ---- 车辆 ------------------------------------------------------------
  struct Car {
    float p0, v;
    int dir;
  };
  std::vector<Car> cars_;
  static constexpr float kBinWidth = 0.2f;
  static constexpr float kCarStartU = -6.0f;
  int bin_count_ = 0;
  std::vector<float> lane_a_, lane_b_;
  std::array<int, kWidth> bin_lo_{}, bin_hi_{};
  std::array<float, kWidth> col_a_{}, col_b_{};

  // ---- 输出 ------------------------------------------------------------
  std::vector<uint8_t> step_, color_;

  void Init();
  void PaintCell(int k, float cr, float cg, float cb, float base_level);
};

#endif  // EXAMPLES_JUST_FOR_FUN_MARINE_DRIVE_H_