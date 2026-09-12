#include "firefox_tab_appearance.h"

QSize FirefoxTabAppearance::adjustSize(const QStyleOptionTab* opt,
                                       const QSize& baseSize) const {
  Q_UNUSED(opt);
  QSize sz = baseSize;
  sz.setWidth(sz.width() + spacing_);
  return sz;
}

QPainterPath FirefoxTabAppearance::shape(const QStyleOptionTab* opt,
                                         const TabTheme& theme) const {
  Q_UNUSED(theme);
  // 在 QTabBar 分配的矩形里居中留白，形成"独立 tab"
  QRectF r = opt->rect;
  r.adjust(spacing_ / 2.0, 2, -spacing_ / 2.0, -2);

  QPainterPath path;
  path.addRoundedRect(r, radius_, radius_);
  return path;
}