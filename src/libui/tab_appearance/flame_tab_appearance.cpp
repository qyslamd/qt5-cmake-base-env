#include "flame_tab_appearance.h"

#include <QLinearGradient>
#include <QtMath>

namespace {

// 火焰基部高度与振幅：选中最旺，悬停次之，普通小火苗
constexpr qreal kSelectedBaseHeight = 10.0;
constexpr qreal kHoveredBaseHeight = 7.0;
constexpr qreal kNormalBaseHeight = 4.0;
constexpr qreal kSelectedAmplitude = 9.0;
constexpr qreal kHoveredAmplitude = 6.0;
constexpr qreal kNormalAmplitude = 3.0;

// 火苗高度的摆动区间占振幅的比例。下限不为 0，火焰才不会周期性地缩没
constexpr qreal kMinHeightRatio = 0.55;
constexpr qreal kHeightSwingRatio = 0.45;

// 相位推进步长，以及各火苗之间的相位错开量与摆动比例
constexpr qreal kPhaseStep = 0.22;
constexpr qreal kLocalPhaseScale = 1.4;
constexpr qreal kLocalPhaseOffset = 1.1;
constexpr qreal kSwayPhaseScale = 0.9;
constexpr qreal kSwayPhaseOffset = 0.7;
constexpr qreal kSwayRatio = 0.08;

// 单个火苗的最大宽度，据此换算数量；再窄的标签也至少留 3 个
constexpr qreal kFlameMaxWidth = 24.0;
constexpr int kMinFlameCount = 3;

// 贝塞尔控制点相对火苗宽度的收腰比例，以及向基部回勾的高度比例
constexpr qreal kControlRatio = 0.15;
constexpr qreal kCurveBulgeRatio = 0.2;

}  // namespace

void FlameTabAppearance::tick() {
  phase_ += kPhaseStep;
  if (phase_ > 2.0 * M_PI) {
    phase_ -= 2.0 * M_PI;
  }
}

QPainterPath FlameTabAppearance::shape(const QStyleOptionTab* opt,
                                       const TabTheme& theme) const {
  Q_UNUSED(theme);
  const QRectF r = opt->rect;
  const bool selected = opt->state & QStyle::State_Selected;
  const bool hovered = opt->state & QStyle::State_MouseOver;

  qreal base_height = kNormalBaseHeight;
  qreal amplitude = kNormalAmplitude;
  if (selected) {
    base_height = kSelectedBaseHeight;
    amplitude = kSelectedAmplitude;
  } else if (hovered) {
    base_height = kHoveredBaseHeight;
    amplitude = kHoveredAmplitude;
  }

  const int flame_count =
      qMax(kMinFlameCount, static_cast<int>(r.width() / kFlameMaxWidth));
  const qreal flame_width = r.width() / flame_count;
  const qreal base_y = r.top() + base_height;

  QPainterPath path;
  path.moveTo(r.left(), r.bottom());
  path.lineTo(r.left(), base_y);

  for (int i = 0; i < flame_count; ++i) {
    const qreal x0 = r.left() + (i * flame_width);
    const qreal x1 = x0 + flame_width;

    // 每个火苗带自己的相位，避免整排同步起落
    const qreal local_phase =
        (phase_ * kLocalPhaseScale) + (i * kLocalPhaseOffset);
    const qreal h =
        amplitude * (kMinHeightRatio + (kHeightSwingRatio * qSin(local_phase)));

    // 尖峰左右轻微摇摆，更像真实火焰。摆幅远小于半个火苗宽，
    // 尖峰不会摆出自己那一格而与邻火苗的曲线交叉
    const qreal sway =
        qSin((phase_ * kSwayPhaseScale) + (i * kSwayPhaseOffset)) *
        flame_width * kSwayRatio;
    const qreal peak_x = x0 + (flame_width * 0.5) + sway;
    const qreal peak_y = r.top() + base_height - h;

    // 左半：从基部升到尖峰；右半：从尖峰落回下一个基部。用三次贝塞尔
    // 把腰身收细，比直线更像火苗
    path.cubicTo(x0 + (flame_width * kControlRatio),
                 base_y - (h * kCurveBulgeRatio),
                 peak_x - (flame_width * kControlRatio),
                 peak_y + (h * kCurveBulgeRatio), peak_x, peak_y);
    path.cubicTo(peak_x + (flame_width * kControlRatio),
                 peak_y + (h * kCurveBulgeRatio),
                 x1 - (flame_width * kControlRatio),
                 base_y - (h * kCurveBulgeRatio), x1, base_y);
  }

  path.lineTo(r.right(), r.bottom());
  path.closeSubpath();
  return path;
}

QBrush FlameTabAppearance::brush(const QStyleOptionTab* opt,
                                 const TabTheme& theme, bool selected,
                                 bool hovered) const {
  Q_UNUSED(theme);
  const QRectF r = opt->rect;

  // 由下往上取色：深红 -> 红 -> 橙 -> 黄，火苗越靠尖越亮。
  // 未选中态透明度很低，只留一个火苗轮廓
  QLinearGradient g(r.bottomLeft(), r.topLeft());
  if (selected) {
    g.setColorAt(0.00, QColor(0xC4, 0x0A, 0x00));
    g.setColorAt(0.35, QColor(0xFF, 0x2A, 0x00));
    g.setColorAt(0.65, QColor(0xFF, 0x8A, 0x00));
    g.setColorAt(1.00, QColor(0xFF, 0xE6, 0x40));
  } else if (hovered) {
    g.setColorAt(0.00, QColor(0xFF, 0x2A, 0x00, 180));
    g.setColorAt(0.50, QColor(0xFF, 0x8A, 0x00, 170));
    g.setColorAt(1.00, QColor(0xFF, 0xE6, 0x40, 160));
  } else {
    g.setColorAt(0.00, QColor(0xFF, 0x2A, 0x00, 70));
    g.setColorAt(0.50, QColor(0xFF, 0x8A, 0x00, 80));
    g.setColorAt(1.00, QColor(0xFF, 0xE6, 0x40, 100));
  }

  return QBrush(g);
}
