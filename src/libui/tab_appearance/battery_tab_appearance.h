#ifndef FILE_LIBUI_TAB_APPEARANCE_BATTERY_TAB_APPEARANCE_H_
#define FILE_LIBUI_TAB_APPEARANCE_BATTERY_TAB_APPEARANCE_H_

#include "tab_appearance.h"

// 电池标签：顶部支出一枚"电池帽"，主体为圆角矩形，两者取并集。
// 填充色按电量走：选中=满电(绿)，悬停=半电(黄)，普通=低电(红)，
// 色相在 0(红)~120(绿) 之间由 QColor::fromHsv 线性插值。无动画。
class BatteryTabAppearance : public TabAppearance {
 public:
  BatteryTabAppearance() = default;

  QPainterPath shape(const QStyleOptionTab* opt,
                     const TabTheme& theme) const override;

  QBrush brush(const QStyleOptionTab* opt, const TabTheme& theme, bool selected,
               bool hovered) const override;

 private:
  // 三态映射到电量 [0, 1]
  static qreal levelFor(bool selected, bool hovered);
};

#endif  // FILE_LIBUI_TAB_APPEARANCE_BATTERY_TAB_APPEARANCE_H_