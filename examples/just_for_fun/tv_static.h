// ============================================================================
//  tv static — 一台旧电视机满屏雪花，一条交流声亮带滚动穿过；
//             旋钮咔哒一下，测试卡滚进画面稳住，然后信号又丢了。
//  原作: github.com/bas3line/ascii (TypeScript, MIT)
//  Qt 5.15.2 C++ 直译
//
//  输出 (QChar, colorIdx) 字符缓冲，配合 TextView 用等宽字体画。
//  字符本身携带几何（/ \ | _ = ( ) o # ░▒▓█ 及 Unicode 制表符），
//  不压成圆点。
// ============================================================================
#ifndef EXAMPLES_JUST_FOR_FUN_TV_STATIC_H_
#define EXAMPLES_JUST_FOR_FUN_TV_STATIC_H_

#include <cstdint>
#include <vector>

#include <QChar>

class TvStatic {
 public:
  static constexpr int kWidth = 58;
  static constexpr int kHeight = 26;
  static constexpr int kPaletteN = 1;

  TvStatic();
  void Update(double t);

  const std::vector<QChar>& charBuf() const { return chars_; }
  const std::vector<uint8_t>& colorBuf() const { return colors_; }

  static const char* const* PaletteHex();

 private:
  // 画面区：电视柜里挖出来的那块屏幕
  static constexpr int kX0 = 4;
  static constexpr int kY0 = 7;
  static constexpr int kSW = 39;
  static constexpr int kSH = 15;

  // 静态预计算
  std::vector<QChar> frame_;     // 电视机柜（含旋钮、喇叭、机脚、天线）
  std::vector<int8_t> card_lv_;  // kSH*kSW，>=0 表示灰阶 0..7
  std::vector<QChar> card_gl_;   // kSH*kSW，当 card_lv_<0 时使用的制表符

  // 每帧输出
  std::vector<QChar> chars_;
  std::vector<uint8_t> colors_;

  void Init();
  void DrawSetFrame();
  void DrawTestCard();
  bool Inside(int c, int r) const;
};

#endif  // EXAMPLES_JUST_FOR_FUN_TV_STATIC_H_