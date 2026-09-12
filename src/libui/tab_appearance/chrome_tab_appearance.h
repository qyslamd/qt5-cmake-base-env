#ifndef FILE_LIBUI_TAB_APPEARANCE_CHROME_TAB_APPEARANCE_H_
#define FILE_LIBUI_TAB_APPEARANCE_CHROME_TAB_APPEARANCE_H_

#include "tab_appearance.h"

// 旧版 Chrome 斜线标签
class ChromeTabAppearance : public TabAppearance {
 public:
  QPainterPath shape(const QStyleOptionTab* opt,
                     const TabTheme& theme) const override;
  bool wantsSeparator(const QStyleOptionTab* opt) const override;
  QLineF separatorLine(const QStyleOptionTab* opt) const override;

 private:
  static constexpr qreal kHRatio = 1.0 / 5.0;  // 斜线的水平偏移比例
  QPainterPath buildShape(const QStyleOptionTab* opt, bool selected) const;
};

#endif  // FILE_LIBUI_TAB_APPEARANCE_CHROME_TAB_APPEARANCE_H_