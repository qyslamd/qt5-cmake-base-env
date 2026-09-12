#ifndef FILE_LIBUI_TAB_APPEARANCE_FLAME_TAB_APPEARANCE_H_
#define FILE_LIBUI_TAB_APPEARANCE_FLAME_TAB_APPEARANCE_H_

#include "tab_appearance.h"

// 火焰标签：顶边由一排火苗的尖峰构成，火苗数量随标签宽度走；选中火最旺、
// 悬停次之。每个火苗带独立相位与摆幅，逐帧跳动。填充为下红上黄的火焰渐变
class FlameTabAppearance : public TabAppearance {
 public:
  FlameTabAppearance() = default;

  QPainterPath shape(const QStyleOptionTab* opt,
                     const TabTheme& theme) const override;

  QBrush brush(const QStyleOptionTab* opt, const TabTheme& theme, bool selected,
               bool hovered) const override;

  bool isAnimated() const override { return true; }
  void tick() override;

 private:
  // 火焰的整体相位，由 tick 推进、shape 读取
  qreal phase_ = 0.0;
};

#endif  // FILE_LIBUI_TAB_APPEARANCE_FLAME_TAB_APPEARANCE_H_
