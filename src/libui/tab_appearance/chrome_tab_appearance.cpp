#include "chrome_tab_appearance.h"

namespace {

// 每个角（左下/右下）的形态
enum class CornerMode { Extend, Normal, Retract };

CornerMode leftCorner(const QStyleOptionTab* opt, bool selected) {
  CornerMode m = CornerMode::Normal;
  switch (opt->position) {
    case QStyleOptionTab::Middle:
    case QStyleOptionTab::End:
      m = CornerMode::Extend;
      break;
    default:
      break;  // Beginning / OnlyOneTab 保持 Normal
  }
  if (!selected &&
      opt->selectedPosition == QStyleOptionTab::PreviousIsSelected) {
    m = CornerMode::Retract;
  }
  return m;
}

CornerMode rightCorner(const QStyleOptionTab* opt, bool selected) {
  CornerMode m = CornerMode::Normal;
  switch (opt->position) {
    case QStyleOptionTab::Beginning:
    case QStyleOptionTab::Middle:
      m = CornerMode::Extend;
      break;
    default:
      break;
  }
  if (!selected && opt->selectedPosition == QStyleOptionTab::NextIsSelected) {
    m = CornerMode::Retract;
  }
  return m;
}

}  // namespace

QPainterPath ChromeTabAppearance::buildShape(const QStyleOptionTab* opt,
                                             bool selected) const {
  const QRectF r = opt->rect;
  const qreal per = r.height() * kHRatio;

  const CornerMode lm = leftCorner(opt, selected);
  const CornerMode rm = rightCorner(opt, selected);

  // 初值取 Normal 形态：覆盖 MSVC 的"可能未初始化"告警（switch 无
  // default 时它不认可枚举已被穷尽）
  qreal p1x = r.left();
  qreal p2x = r.left() + per;
  switch (lm) {
    case CornerMode::Extend:
      p1x = r.left() - per;
      p2x = r.left();
      break;
    case CornerMode::Normal:
      p1x = r.left();
      p2x = r.left() + per;
      break;
    case CornerMode::Retract:
      p1x = r.left() + per;
      p2x = r.left();
      break;
  }

  qreal p8x = r.right();
  qreal p7x = r.right() - per;
  switch (rm) {
    case CornerMode::Extend:
      p8x = r.right() + per;
      p7x = r.right();
      break;
    case CornerMode::Normal:
      p8x = r.right();
      p7x = r.right() - per;
      break;
    case CornerMode::Retract:
      p8x = r.right() - per;
      p7x = r.right();
      break;
  }

  const QPointF p1(p1x, r.bottom());
  const QPointF p2(p2x, r.bottom() - per);
  const QPointF p3(p2x, r.top() + per);
  const QPointF p4(p2x + per, r.top());
  const QPointF p5(p7x - per, r.top());
  const QPointF p6(p7x, r.top() + per);
  const QPointF p7(p7x, r.bottom() - per);
  const QPointF p8(p8x, r.bottom());

  QPainterPath path;
  path.moveTo(p1);
  path.quadTo(QPointF(p2.x(), p1.y()), p2);  // 左下斜线
  path.lineTo(p3);
  path.quadTo(QPointF(p3.x(), p4.y()), p4);  // 左上圆角
  path.lineTo(p5);
  path.quadTo(QPointF(p6.x(), p5.y()), p6);  // 右上圆角
  path.lineTo(p7);
  path.quadTo(QPointF(p7.x(), p8.y()), p8);  // 右下斜线
  path.lineTo(p1);
  return path;
}

QPainterPath ChromeTabAppearance::shape(const QStyleOptionTab* opt,
                                        const TabTheme& theme) const {
  Q_UNUSED(theme);
  const bool selected = opt->state & QStyle::State_Selected;
  return buildShape(opt, selected);
}

bool ChromeTabAppearance::wantsSeparator(const QStyleOptionTab* opt) const {
  return !(opt->state & QStyle::State_Selected) &&
         !(opt->state & QStyle::State_MouseOver);
}

QLineF ChromeTabAppearance::separatorLine(const QStyleOptionTab* opt) const {
  const QRectF r = opt->rect;
  const qreal mar = r.height() / 4.0;
  switch (opt->position) {
    case QStyleOptionTab::Beginning:
    case QStyleOptionTab::Middle:
      return QLineF(r.right(), r.top() + mar, r.right(), r.bottom() - mar);
    default:
      return QLineF();
  }
}