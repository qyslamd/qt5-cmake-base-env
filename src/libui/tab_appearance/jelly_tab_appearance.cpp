#include "jelly_tab_appearance.h"

#include <QLinearGradient>
#include <QtMath>

namespace {

// 波浪振幅：选中最弹，悬停次之，普通最平；频率同理由选中态拉开
constexpr qreal kSelectedAmplitude = 5.0;
constexpr qreal kHoveredAmplitude = 3.5;
constexpr qreal kNormalAmplitude = 1.5;
constexpr qreal kSelectedFrequency = 3.0;
constexpr qreal kNormalFrequency = 2.0;

// 相位推进步长
constexpr qreal kPhaseStep = 0.16;

// 波峰最多抬到振幅的这个比例，留出余量避免顶出 rect 被裁
constexpr qreal kWaveFillRatio = 0.6;

}  // namespace

void JellyTabAppearance::tick() {
  phase_ += kPhaseStep;
  if (phase_ > 2.0 * M_PI) {
    phase_ -= 2.0 * M_PI;
  }
}

QPainterPath JellyTabAppearance::shape(const QStyleOptionTab* opt,
                                       const TabTheme& theme) const {
  Q_UNUSED(theme);
  const QRectF r = opt->rect;
  const bool selected = opt->state & QStyle::State_Selected;
  const bool hovered = opt->state & QStyle::State_MouseOver;

  qreal amplitude = kNormalAmplitude;
  qreal frequency = kNormalFrequency;
  if (selected) {
    amplitude = kSelectedAmplitude;
    frequency = kSelectedFrequency;
  } else if (hovered) {
    amplitude = kHoveredAmplitude;
  }

  QPainterPath path;
  path.moveTo(r.left(), r.bottom());
  path.lineTo(r.left(), r.top() + amplitude);

  // 顶边逐点起伏。两端用包络衰减到 0，使起点与终点都贴在顶边上，
  // 中间起伏最强且不外溢
  const int steps = qMax(30, static_cast<int>(r.width() / 3.0));
  for (int i = 1; i <= steps; ++i) {
    const qreal t = qreal(i) / steps;
    const qreal envelope = qSin(t * M_PI);
    const qreal wave = qSin((phase_ * frequency) + (t * M_PI * 2 * frequency));
    const qreal y =
        r.top() + amplitude - (envelope * wave * amplitude * kWaveFillRatio);
    path.lineTo(r.left() + (t * r.width()), y);
  }

  path.lineTo(r.right(), r.bottom());
  path.closeSubpath();
  return path;
}

QBrush JellyTabAppearance::brush(const QStyleOptionTab* opt,
                                 const TabTheme& theme, bool selected,
                                 bool hovered) const {
  Q_UNUSED(theme);
  const QRectF r = opt->rect;

  if (selected) {
    // 果冻质感：上下高光渐变
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, QColor(0xFF, 0x9A, 0xC8));
    g.setColorAt(0.5, QColor(0xFF, 0x6F, 0xA8));
    g.setColorAt(1.0, QColor(0xE0, 0x4A, 0x8A));
    return QBrush(g);
  }
  if (hovered) {
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, QColor(0xFF, 0xC2, 0xDD, 180));
    g.setColorAt(1.0, QColor(0xFF, 0x8F, 0xBE, 220));
    return QBrush(g);
  }
  // 未选中：极淡的粉底，否则波浪在普通态看不出来
  return QBrush(QColor(0xFF, 0xE4, 0xEE, 100));
}
