#ifndef FILE_LIBUI_TAB_APPEARANCE_JELLY_TAB_APPEARANCE_H_
#define FILE_LIBUI_TAB_APPEARANCE_JELLY_TAB_APPEARANCE_H_

#include "tab_appearance.h"

// 果冻标签：顶边按正弦起伏，像果冻在弹；选中振幅最大、悬停次之。
// 填充为粉色高光渐变
class JellyTabAppearance : public TabAppearance {
 public:
  JellyTabAppearance() = default;

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

#endif  // FILE_LIBUI_TAB_APPEARANCE_JELLY_TAB_APPEARANCE_H_
