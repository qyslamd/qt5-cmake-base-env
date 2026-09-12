#ifndef FILE_LIBUI_TAB_APPEARANCE_NEON_TAB_APPEARANCE_H_
#define FILE_LIBUI_TAB_APPEARANCE_NEON_TAB_APPEARANCE_H_

#include "tab_appearance.h"

// 霓虹标签：整条标签铺深色底，再用多层描边叠出灯管的辉光，选中时呼吸。
// 辉光由多层描边构成，画刷表达不了，故覆盖 paint() 而不是 brush()
class NeonTabAppearance : public TabAppearance {
 public:
  NeonTabAppearance() = default;

  QPainterPath shape(const QStyleOptionTab* opt,
                     const TabTheme& theme) const override;

  void paint(QPainter* p, const QStyleOptionTab* opt, const TabTheme& theme,
             bool selected, bool hovered) const override;

  bool isAnimated() const override { return true; }
  void tick() override;

 private:
  // 呼吸相位，由 tick 推进、paint 读取
  qreal phase_ = 0.0;
};

#endif  // FILE_LIBUI_TAB_APPEARANCE_NEON_TAB_APPEARANCE_H_
