// ============================================================================
//  alpine dawn — 雪峰第一缕晨光与静湖倒影，逐格半色调终端艺术
//  原作: github.com/bas3line/ascii (TypeScript, MIT)
//  Qt 5.15.2 C++ 直译
// ============================================================================
#ifndef EXAMPLES_JUST_FOR_FUN_ALPINE_DAWN_H_
#define EXAMPLES_JUST_FOR_FUN_ALPINE_DAWN_H_

#include <array>
#include <cstdint>
#include <vector>

#include "halftone_common.h"

class AlpineDawn {
 public:
  static constexpr int kWidth = 200;
  static constexpr int kHeight = 100;
  static constexpr int kPaletteN = 46;

  AlpineDawn();
  void Update(double t);

  const std::vector<uint8_t>& stepBuf() const { return step_; }
  const std::vector<uint8_t>& colorBuf() const { return color_; }

  static const char* const* PaletteHex();

 private:
  // ---- 调色板 ----------------------------------------------------------
  HalftonePalette palette_;

  // ---- 静态预计算（一次性） -------------------------------------------
  //  kShoreRow = SHORE 行，为地平线以上的部分
  static constexpr int kShoreRow = 62;
  static constexpr int kLakeRow = kHeight - kShoreRow;        // 38
  static constexpr int kMistW = 480;                          // 雾层环绕宽度
  static constexpr int kMistTop = 36;                         // 雾层起始行
  static constexpr int kMistRows = kShoreRow + 2 - kMistTop;  // 28
  static constexpr int kCloudW = 640;                         // 云层环绕宽度
  static constexpr int kCloudRows = 34;                       // 云层行数

  std::vector<float> depth_;     // kShoreRow*kWidth  光线命中距离（0 = 天空）
  std::vector<float> alt_;       // kShoreRow*kWidth  命中点高度
  std::vector<float> sun_;       // kShoreRow*kWidth  太阳光照
  std::vector<float> snow_;      // kShoreRow*kWidth  积雪覆盖
  std::vector<float> up_;        // kShoreRow*kWidth  法线 y 分量
  std::vector<uint8_t> rim_;     // kShoreRow*kWidth  山脊线
  std::vector<float> tree_top_;  // kWidth     远岸树线
  std::vector<float> src_;       // kLakeRow*kWidth  湖面每格反射采样行
  std::vector<uint8_t> fg_;      // N     前景分类（0 无 / 1 岸边 / 2 松树）
  std::vector<float> fg_shade_;  // N
  std::vector<float> fg_rim_;    // N
  std::vector<float> mist_;      // kMistW*kMistRows
  std::vector<float> cloud_;     // kCloudW*kCloudRows
  std::vector<float> grain_;     // N     全域低频噪声
  std::vector<float> sky_r_, sky_g_, sky_b_;  // kShoreRow*kWidth
  std::vector<float> glow_a_;                 // kShoreRow*kWidth  太阳辉光

  // ---- 每帧缓冲 --------------------------------------------------------
  std::vector<float> a_r_, a_g_, a_b_;  // kShoreRow*kWidth  反射源
  std::vector<float> f_r_, f_g_, f_b_;  // N
  std::vector<float> flo_, fade_;       // N
  std::vector<float> wisp_;             // kWidth
  std::vector<uint8_t> step_, color_;   // N

  void Init();
};

#endif  // EXAMPLES_JUST_FOR_FUN_ALPINE_DAWN_H_