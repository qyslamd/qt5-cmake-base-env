#include "cat_ear_tab_appearance.h"

#include <QLinearGradient>
#include <QtMath>

namespace {

// 耳朵基准高：选中最高，悬停次之，普通最矮
constexpr qreal kSelectedEarHeight = 12.0;
constexpr qreal kHoveredEarHeight = 9.0;
constexpr qreal kNormalEarHeight = 6.0;

// 呼吸振幅与相位推进步长
constexpr qreal kBreathAmplitude = 1.0;
constexpr qreal kPhaseStep = 0.12;

// 耳底宽，以及耳底距左右边缘的占比
constexpr qreal kEarWidth = 10.0;
constexpr qreal kEarInsetRatio = 0.25;

// 主体矩形圆角
constexpr qreal kBodyRadius = 4.0;

}  // namespace

void CatEarTabAppearance::tick() {
  phase_ += kPhaseStep;
  if (phase_ > 2.0 * M_PI) {
    phase_ -= 2.0 * M_PI;
  }
}

QPainterPath CatEarTabAppearance::shape(const QStyleOptionTab* opt,
                                        const TabTheme& theme) const {
  Q_UNUSED(theme);
  const QRectF r = opt->rect;
  const bool selected = opt->state & QStyle::State_Selected;
  const bool hovered = opt->state & QStyle::State_MouseOver;

  qreal ear_height = kNormalEarHeight;
  if (selected) {
    ear_height = kSelectedEarHeight;
  } else if (hovered) {
    ear_height = kHoveredEarHeight;
  }
  ear_height += qSin(phase_) * kBreathAmplitude;

  // 耳朵要留在 rect 内，否则会被相邻标签的裁剪区削平，故主体整体
  // 下移耳朵的高度
  const qreal ear_inset = r.width() * kEarInsetRatio;
  const QRectF body(r.left(), r.top() + ear_height, r.width(),
                    r.height() - ear_height);

  QPainterPath path;
  path.moveTo(body.left() + kBodyRadius, body.top());

  // 左耳：顶点落在耳底的外侧，右耳与之关于中轴镜像
  path.lineTo(body.left() + ear_inset, body.top() - (ear_height * 0.9));
  path.lineTo(body.left() + ear_inset + kEarWidth, body.top());

  // 两耳之间的顶边
  path.lineTo(body.right() - ear_inset - kEarWidth, body.top());

  // 右耳
  path.lineTo(body.right() - ear_inset, body.top() - (ear_height * 0.9));
  path.lineTo(body.right() - ear_inset, body.top());

  // 右上圆角
  path.lineTo(body.right() - kBodyRadius, body.top());
  path.quadTo(body.right(), body.top(), body.right(), body.top() + kBodyRadius);

  // 右边缘、底边、左边缘
  path.lineTo(body.right(), body.bottom());
  path.lineTo(body.left(), body.bottom());
  path.lineTo(body.left(), body.top() + kBodyRadius);

  // 左上圆角
  path.quadTo(body.left(), body.top(), body.left() + kBodyRadius, body.top());

  path.closeSubpath();
  return path;
}

QBrush CatEarTabAppearance::brush(const QStyleOptionTab* opt,
                                  const TabTheme& theme, bool selected,
                                  bool hovered) const {
  Q_UNUSED(theme);
  const QRectF r = opt->rect;

  if (selected) {
    // 猫咪毛色：暖橘渐变
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, QColor(0xFF, 0xD9, 0xA0));
    g.setColorAt(1.0, QColor(0xF5, 0xA6, 0x5B));
    return QBrush(g);
  }
  if (hovered) {
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, QColor(0xFF, 0xEF, 0xD0, 220));
    g.setColorAt(1.0, QColor(0xFF, 0xCC, 0x99, 230));
    return QBrush(g);
  }
  // 未选中：米色底，让耳朵轮廓在普通态也看得出来
  return QBrush(QColor(0xFC, 0xEC, 0xD6, 200));
}
