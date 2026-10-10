// ============================================================================
//  hourglass — 立架上的沙漏：沙子一粒粒穿过颈部，上坑下堆；
//              上球流光后被抬起、侧翻、倒放回来
//  原作: github.com/bas3line/ascii (TypeScript, MIT)
//  Qt 5.15.2 C++ 直译
//
//  输出 (QChar, colorIdx) 字符缓冲，配合 TextView 用等宽字体画。
//  玻璃轮廓用 ' - . _ / \ | = ( )，画框用 Unicode box-drawing，
//  沙子用 ' . :，全部保留原字符形状。
// ============================================================================
#ifndef EXAMPLES_JUST_FOR_FUN_HOURGLASS_H_
#define EXAMPLES_JUST_FOR_FUN_HOURGLASS_H_

#include <array>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include <QChar>

class Hourglass {
 public:
  static constexpr int kWidth = 43;
  static constexpr int kHeight = 21;
  static constexpr int kPaletteN = 2;

  Hourglass();
  void Update(double t);

  const std::vector<QChar>& charBuf() const { return chars_; }
  const std::vector<uint8_t>& colorBuf() const { return colors_; }

  static const char* const* PaletteHex();

 private:
  // 三种玻璃网格：直立（落）、直立（抬起）、侧翻
  std::array<std::vector<QChar>, 3> shells_;

  // 每帧缓冲
  std::vector<QChar> chars_;
  std::vector<uint8_t> colors_;
  std::vector<uint8_t> bits_;  // 沙子覆盖：bit 0 上半，bit 1 下半

  void Init();
  std::vector<QChar> MakeShell(bool side, int lift);
  static void TraceLine(const std::vector<std::pair<float, float>>& pts,
                        const std::function<void(int, int, QChar)>& put);
};

#endif  // EXAMPLES_JUST_FOR_FUN_HOURGLASS_H_