#ifndef FILE_LIBUI_TAB_APPEARANCE_TAB_BAR_STYLE_H_
#define FILE_LIBUI_TAB_APPEARANCE_TAB_BAR_STYLE_H_

#include <QProxyStyle>

#include "tab_appearance.h"

class TabBarStyle : public QProxyStyle {
  Q_OBJECT
 public:
  explicit TabBarStyle(QObject* parent = nullptr);
  TabBarStyle(TabAppearance* appearance, QObject* parent = nullptr);

  // 替换外观（接管所有权）
  void setAppearance(TabAppearance* appearance);
  TabAppearance* appearance() const { return appearance_; }

  // 主题可读可写
  TabTheme& theme() { return theme_; }
  const TabTheme& theme() const { return theme_; }

  QSize sizeFromContents(ContentsType type, const QStyleOption* opt,
                         const QSize& size, const QWidget* w) const override;

  void drawControl(ControlElement element, const QStyleOption* opt, QPainter* p,
                   const QWidget* w) const override;

 private:
  TabAppearance* appearance_ = nullptr;
  TabTheme theme_;
};

#endif  // FILE_LIBUI_TAB_APPEARANCE_TAB_BAR_STYLE_H_