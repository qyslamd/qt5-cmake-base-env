#ifndef FILE_LIBUI_TAB_APPEARANCE_CAT_EAR_TAB_APPEARANCE_H_
#define FILE_LIBUI_TAB_APPEARANCE_CAT_EAR_TAB_APPEARANCE_H_

#include "tab_appearance.h"

// 猫耳标签：顶边两侧各支出一只三角耳，选中耳朵最高、悬停次之，
// 再叠一层正弦呼吸让耳朵微微起伏。填充为暖橘渐变
class CatEarTabAppearance : public TabAppearance {
 public:
  CatEarTabAppearance() = default;

  QPainterPath shape(const QStyleOptionTab* opt,
                     const TabTheme& theme) const override;

  QBrush brush(const QStyleOptionTab* opt, const TabTheme& theme, bool selected,
               bool hovered) const override;

  bool isAnimated() const override { return true; }
  void tick() override;

 private:
  // 耳朵呼吸的相位，由 tick 推进、shape 读取
  qreal phase_ = 0.0;
};

#endif  // FILE_LIBUI_TAB_APPEARANCE_CAT_EAR_TAB_APPEARANCE_H_
