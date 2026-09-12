#ifndef FILE_LIBUI_TAB_APPEARANCE_FIREFOX_TAB_APPEARANCE_H_
#define FILE_LIBUI_TAB_APPEARANCE_FIREFOX_TAB_APPEARANCE_H_

#include "tab_appearance.h"

// Firefox Proton 风格：独立圆角矩形，带间距
class FirefoxTabAppearance : public TabAppearance {
 public:
  explicit FirefoxTabAppearance(int spacing = 4, int radius = 8)
      : spacing_(spacing), radius_(radius) {}

  QSize adjustSize(const QStyleOptionTab* opt,
                   const QSize& baseSize) const override;
  QPainterPath shape(const QStyleOptionTab* opt,
                     const TabTheme& theme) const override;

 private:
  int spacing_;
  int radius_;
};

#endif  // FILE_LIBUI_TAB_APPEARANCE_FIREFOX_TAB_APPEARANCE_H_