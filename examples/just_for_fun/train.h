// ============================================================================
//  train — 一台蒸汽机车拖着一节车厢和一节煤水车横穿画面：
//          烟囱随轮轴的节拍一股股喷烟，连杆随车轮一起转；
//          从右侧驶出画面，再绕回来。
//  原作: github.com/bas3line/ascii (TypeScript, MIT)
//  Qt 5.15.2 C++ 直译
//
//  输出 (QChar, colorIdx) 字符缓冲，配合 TextView 用等宽字体画。
//  字符本身携带几何（/ \ | _ = ( ) o # 等），不压成圆点。
// ============================================================================
#ifndef EXAMPLES_JUST_FOR_FUN_TRAIN_H_
#define EXAMPLES_JUST_FOR_FUN_TRAIN_H_

#include <cstdint>
#include <vector>

#include <QChar>

class Train {
 public:
  static constexpr int kWidth = 76;
  static constexpr int kHeight = 26;
  static constexpr int kPaletteN = 1;

  Train();
  void Update(double t);

  const std::vector<QChar>& charBuf() const { return chars_; }
  const std::vector<uint8_t>& colorBuf() const { return colors_; }

  static const char* const* PaletteHex();

 private:
  // 每帧输出
  std::vector<QChar> chars_;
  std::vector<uint8_t> colors_;
  std::vector<float> dens_;  // 烟密度累积缓冲

  void Init();
  void Put(int r, int c, QChar ch);
  void PutStr(int r, int c0, const char* s);
};

#endif  // EXAMPLES_JUST_FOR_FUN_TRAIN_H_