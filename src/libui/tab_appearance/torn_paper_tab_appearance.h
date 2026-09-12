#ifndef FILE_LIBUI_TAB_APPEARANCE_TORN_PAPER_TAB_APPEARANCE_H_
#define FILE_LIBUI_TAB_APPEARANCE_TORN_PAPER_TAB_APPEARANCE_H_

#include "tab_appearance.h"

// 撕纸标签：四边都是毛边，像是从纸上撕下来的一角；选中时撕痕更大。
// 填充为泛黄的纸色，无动画，撕痕是静态的
class TornPaperTabAppearance : public TabAppearance {
 public:
  TornPaperTabAppearance() = default;

  QPainterPath shape(const QStyleOptionTab* opt,
                     const TabTheme& theme) const override;

  QBrush brush(const QStyleOptionTab* opt, const TabTheme& theme, bool selected,
               bool hovered) const override;

 private:
  // 用固定种子的伪随机，保证每次绘制形状一致
  static qreal pseudoRandom(quint32& seed);
};

#endif  // FILE_LIBUI_TAB_APPEARANCE_TORN_PAPER_TAB_APPEARANCE_H_
