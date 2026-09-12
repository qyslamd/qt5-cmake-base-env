#include "torn_paper_tab_appearance.h"

#include <QLinearGradient>

qreal TornPaperTabAppearance::pseudoRandom(quint32& seed) {
  seed = (seed * 1664525u) + 1013904223u;
  return ((seed >> 8) & 0xFFFF) / qreal(0xFFFF);
}

QPainterPath TornPaperTabAppearance::shape(const QStyleOptionTab* opt,
                                           const TabTheme& theme) const {
  Q_UNUSED(theme);
  const QRectF r = opt->rect;
  const bool selected = opt->state & QStyle::State_Selected;

  // 种子与 tab 的 x 坐标绑定，否则所有 tab 会共用同一条撕痕
  quint32 seed = 0x9E3779B9u ^ quint32(qRound(r.left()));

  // 撕裂幅度：选中更夸张一点
  const qreal tear_top = selected ? 5.0 : 3.5;
  const qreal tear_bottom = selected ? 3.0 : 2.0;

  QPainterPath path;
  path.moveTo(r.left(), r.bottom());

  // 左边缘：轻微毛刺
  {
    qreal y = r.bottom();
    while (y > r.top() + 5) {
      y -= 3.0 + (pseudoRandom(seed) * 4.0);
      const qreal x = r.left() + ((pseudoRandom(seed) - 0.5) * 2.0);
      path.lineTo(x, y);
    }
  }

  // 顶边：从左上到右上，锯齿明显
  {
    qreal x = r.left();
    const qreal y_base = r.top() + tear_top;
    while (x < r.right() - 3) {
      x += 3.0 + (pseudoRandom(seed) * 6.0);
      const qreal y = y_base + ((pseudoRandom(seed) - 0.5) * tear_top * 2.0);
      path.lineTo(qMin(x, r.right()), y);
    }
  }

  // 右边缘：轻微毛刺
  {
    qreal y = r.top() + tear_top;
    while (y < r.bottom() - 5) {
      y += 3.0 + (pseudoRandom(seed) * 4.0);
      const qreal x = r.right() + ((pseudoRandom(seed) - 0.5) * 2.0);
      path.lineTo(x, qMin(y, r.bottom()));
    }
  }

  // 底边：轻微起伏，作为纸张的下缘
  {
    qreal x = r.right();
    const qreal y_base = r.bottom() - tear_bottom;
    while (x > r.left() + 3) {
      x -= 4.0 + (pseudoRandom(seed) * 5.0);
      const qreal y = y_base + ((pseudoRandom(seed) - 0.5) * tear_bottom);
      path.lineTo(qMax(x, r.left()), y);
    }
  }

  path.closeSubpath();
  return path;
}

QBrush TornPaperTabAppearance::brush(const QStyleOptionTab* opt,
                                     const TabTheme& theme, bool selected,
                                     bool hovered) const {
  Q_UNUSED(theme);
  const QRectF r = opt->rect;

  if (selected) {
    // 泛黄纸张 + 一点阴影感
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, QColor(0xFF, 0xF6, 0xD8));
    g.setColorAt(1.0, QColor(0xE8, 0xD9, 0xA8));
    return QBrush(g);
  }
  if (hovered) {
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, QColor(0xFF, 0xFC, 0xEC));
    g.setColorAt(1.0, QColor(0xF4, 0xEA, 0xC6));
    return QBrush(g);
  }
  // TabBarStyle 在既未选中也未悬停时不取 brush，此处无需填充
  return QBrush(Qt::NoBrush);
}
