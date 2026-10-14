#include "password_eye_button.h"

#include <QPainter>
#include <QPainterPath>

namespace {

constexpr int kSize = 24;      // 正方形按钮，图标 18x18
constexpr qreal kInset = 3.0;  // 眼形到按钮边缘的留白
constexpr qreal kStroke = 1.6;
constexpr qreal kPupilRadius = 2.0;

}  // namespace

PasswordEyeButton::PasswordEyeButton(QWidget* parent)
    : QAbstractButton(parent) {
  setCheckable(true);
  setCursor(Qt::PointingHandCursor);
  setFixedSize(kSize, kSize);
}

void PasswordEyeButton::setEyeColor(const QColor& c) {
  if (eye_color_ == c) {
    return;
  }
  eye_color_ = c;
  update();
}

void PasswordEyeButton::setHoverEyeColor(const QColor& c) {
  if (hover_eye_color_ == c) {
    return;
  }
  hover_eye_color_ = c;
  update();
}

void PasswordEyeButton::enterEvent(QEvent* event) {
  QAbstractButton::enterEvent(event);
  hovered_ = true;
  update();
}

void PasswordEyeButton::leaveEvent(QEvent* event) {
  QAbstractButton::leaveEvent(event);
  hovered_ = false;
  update();
}

QSize PasswordEyeButton::sizeHint() const { return QSize(kSize, kSize); }

void PasswordEyeButton::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  const QColor color = hovered_ ? hover_eye_color_ : eye_color_;
  const QRectF box = QRectF(rect()).adjusted(kInset, kInset, -kInset, -kInset);
  const QPointF center = box.center();
  // 眼形：上下对称的两条二次曲线，中间最宽
  const qreal lift = box.height() * 0.47;
  QPainterPath lens;
  lens.moveTo(box.left(), center.y());
  lens.quadTo(center.x(), center.y() - lift, box.right(), center.y());
  lens.quadTo(center.x(), center.y() + lift, box.left(), center.y());

  QPen pen(color, kStroke);
  pen.setCapStyle(Qt::RoundCap);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  p.drawPath(lens);

  p.setBrush(color);
  p.drawEllipse(center, kPupilRadius, kPupilRadius);

  // 未勾选（密码隐藏）盖一道斜杠
  if (!isChecked()) {
    p.setPen(pen);
    p.drawLine(QPointF(box.left() + 2.0, box.bottom() + 1.0),
               QPointF(box.right() - 2.0, box.top() - 1.0));
  }
}
