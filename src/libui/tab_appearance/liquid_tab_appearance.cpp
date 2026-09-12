#include "liquid_tab_appearance.h"

#include <QLinearGradient>
#include <QRectF>
#include <QtMath>

namespace {

// 波长固定，让不同宽度的标签保持相同的波纹密度
constexpr qreal kWaveLength = 60.0;

}  // namespace

void LiquidTabAppearance::tick() {
  phase_ += 0.18;
  if (phase_ > 2 * M_PI) {
    phase_ -= 2 * M_PI;
  }
}

QPainterPath LiquidTabAppearance::shape(const QStyleOptionTab* opt,
                                        const TabTheme& theme) const {
  Q_UNUSED(theme);

  const QRectF r = opt->rect;
  const bool selected = opt->state & QStyle::State_Selected;
  const bool hovered = opt->state & QStyle::State_MouseOver;

  // 波位三档：选中波位最高（露出更多渐变），悬停居中，普通最低
  qreal base = 12.0;
  qreal amp = 2.0;
  if (selected) {
    base = 6.0;
    amp = 5.0;
  } else if (hovered) {
    base = 9.0;
    amp = 3.0;
  }

  const qreal k = 2.0 * M_PI / kWaveLength;

  QPainterPath path;
  path.moveTo(r.left(), r.bottom());
  path.lineTo(r.left(), r.top() + base);

  const int steps = qMax(20, static_cast<int>(r.width() / 3.0));
  for (int i = 1; i <= steps; ++i) {
    const qreal t = qreal(i) / steps;
    const qreal x = r.left() + (t * r.width());
    const qreal y =
        r.top() + base + (amp * qSin((k * (x - r.left())) + phase_));
    path.lineTo(x, y);
  }

  path.lineTo(r.right(), r.bottom());
  path.closeSubpath();
  return path;
}

QBrush LiquidTabAppearance::brush(const QStyleOptionTab* opt,
                                  const TabTheme& theme, bool selected,
                                  bool hovered) const {
  Q_UNUSED(theme);
  const QRectF r = opt->rect;

  if (selected) {
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, QColor(0x4F, 0xAC, 0xFE));
    g.setColorAt(1.0, QColor(0x00, 0xF2, 0xFE));
    return QBrush(g);
  }
  if (hovered) {
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, QColor(0x4F, 0xAC, 0xFE, 120));
    g.setColorAt(1.0, QColor(0x00, 0xF2, 0xFE, 200));
    return QBrush(g);
  }
  return QBrush(Qt::NoBrush);
}
