// ============================================================================
//  big text — 手工五行块状字体的横幅，双线阴影与扫光，逐格半色调终端艺术
//  原作: github.com/bas3line/ascii (TypeScript, MIT)
//  Qt 5.15.2 C++ 直译
// ============================================================================
#ifndef EXAMPLES_JUST_FOR_FUN_BIG_TEXT_H_
#define EXAMPLES_JUST_FOR_FUN_BIG_TEXT_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class BigText {
 public:
  static constexpr int kWidth = 66;
  static constexpr int kHeight = 8;
  static constexpr int kPaletteN = 4;

  explicit BigText(const std::string& text = "ENPHT");
  void Update(double t);

  const std::vector<uint8_t>& stepBuf() const { return step_; }
  const std::vector<uint8_t>& colorBuf() const { return color_; }

  static const char* const* PaletteHex();

 private:
  // ---- 调色板 ----------------------------------------------------------
  std::array<std::array<float, 3>, kPaletteN> palette_;

  // ---- 静态网格 --------------------------------------------------------
  // 0 空 / 1 阴影 / 2 字形墨
  std::vector<uint8_t> kind_;

  // 扫光的左右端点（列坐标）
  int x_left_ = 0;
  int x_right_ = 0;

  // ---- 输出 ------------------------------------------------------------
  std::vector<uint8_t> step_, color_;

  void Init(const std::string& text);
};

#endif  // EXAMPLES_JUST_FOR_FUN_BIG_TEXT_H_