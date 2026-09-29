#include "widgets/switch_button.h"

#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

SwitchButton::SwitchButton(QWidget* parent) : QAbstractButton(parent) {
  setCursor(Qt::PointingHandCursor);
  setCheckable(true);

  knob_animation_ = new QVariantAnimation(this);
  knob_animation_->setDuration(180);
  knob_animation_->setEasingCurve(QEasingCurve::InOutCubic);
  connect(knob_animation_, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& value) {
            knob_position_ = value.toReal();
            update();
          });
  connect(this, &QAbstractButton::toggled, this,
          &SwitchButton::startKnobAnimation);
}

void SwitchButton::setOnBackground(const QBrush& brush) {
  background_on_ = brush;
  update();
}

void SwitchButton::setOffBackground(const QBrush& brush) {
  background_off_ = brush;
  update();
}

void SwitchButton::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHints(QPainter::Antialiasing);

  // 胶囊底：开/关背景色
  fillRoundRect(&painter, rect(),
                isChecked() ? background_on_ : background_off_);

  // 白色圆形滑块（附轻微投影）
  // 几何做钳制：控件退化尺寸（高度过小或宽度小于直径）时不产生负直径
  const qreal diameter = qMax<qreal>(4.0, height() - 4.0);
  const qreal left = 2.0;
  const qreal right = qMax(left, width() - diameter - 2.0);
  const qreal x = left + (right - left) * knob_position_;
  const qreal y = (height() - diameter) / 2.0;  // 垂直居中

  fillRoundRect(&painter, QRectF(x, y + 1.0, diameter, diameter),
                QColor(0, 0, 0, 46));
  fillRoundRect(&painter, QRectF(x, y, diameter, diameter), Qt::white);
}

QSize SwitchButton::sizeHint() const { return QSize(44, 24); }

QSize SwitchButton::minimumSizeHint() const { return QSize(44, 24); }

void SwitchButton::fillRoundRect(QPainter* painter, const QRectF& rect,
                                 const QBrush& brush) {
  painter->save();
  QPainterPath path;
  path.addRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);
  painter->fillPath(path, brush);
  painter->restore();
}

void SwitchButton::startKnobAnimation(bool checked) {
  knob_animation_->stop();
  knob_animation_->setStartValue(knob_position_);
  knob_animation_->setEndValue(checked ? 1.0 : 0.0);
  knob_animation_->start();
}
