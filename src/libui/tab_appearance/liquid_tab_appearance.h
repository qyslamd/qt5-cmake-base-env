#ifndef FILE_LIBUI_TAB_APPEARANCE_LIQUID_TAB_APPEARANCE_H_
#define FILE_LIBUI_TAB_APPEARANCE_LIQUID_TAB_APPEARANCE_H_

#include "tab_appearance.h"

// 液态波浪标签：顶部是流动的正弦波，选中态波位最高、悬停居中、其余最低。
// 填充为蓝青渐变，不取 TabTheme 的选中色——渐变是本外观的一部分，
// 这也是 brush() 可被子类接管的意义所在
class LiquidTabAppearance : public TabAppearance {
 public:
  LiquidTabAppearance() = default;

  QPainterPath shape(const QStyleOptionTab* opt,
                     const TabTheme& theme) const override;

  QBrush brush(const QStyleOptionTab* opt, const TabTheme& theme, bool selected,
               bool hovered) const override;

  bool isAnimated() const override { return true; }
  void tick() override;

 private:
  // 波形相位，由 tick 推进、shape 读取
  qreal phase_ = 0.0;
};

#endif  // FILE_LIBUI_TAB_APPEARANCE_LIQUID_TAB_APPEARANCE_H_
