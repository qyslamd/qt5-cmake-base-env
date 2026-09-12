#include "progress_bar_base.h"

#include <QHideEvent>
#include <QShowEvent>
#include <QTimer>

namespace {

// 动画帧间隔，约 33 fps；与 TabBarStyle 同值，相位推进快慢由子类的
// tick() 决定
constexpr int kAnimationIntervalMs = 30;

// 尺寸提示：四种风格都是圆形构图，故取正方形。下限留到还看得清细节为止
constexpr int kPreferredSide = 120;
constexpr int kMinimumSide = 48;

}  // namespace

ProgressBase::ProgressBase(QWidget* parent)
    : QWidget(parent), timer_(new QTimer(this)) {
  timer_->setInterval(kAnimationIntervalMs);
  connect(timer_, &QTimer::timeout, this, [this]() {
    // isAnimated() 可能被子类的状态变化改掉而没人通知基类，这里兜一道：
    // 已经不需要动画就停下，别让定时器空转
    if (!isAnimated()) {
      timer_->stop();
      return;
    }
    tick();
    update();
  });
}

ProgressBase::~ProgressBase() = default;

qreal ProgressBase::ratio() const {
  if (maximum_ <= minimum_) {
    return 0.0;
  }
  const qreal r = qreal(value_ - minimum_) / qreal(maximum_ - minimum_);
  return qBound(0.0, r, 1.0);
}

QSize ProgressBase::sizeHint() const {
  return QSize(kPreferredSide, kPreferredSide);
}

QSize ProgressBase::minimumSizeHint() const {
  return QSize(kMinimumSide, kMinimumSide);
}

void ProgressBase::setValue(int value) {
  value = qBound(minimum_, value, maximum_);
  if (value_ == value) {
    return;
  }
  value_ = value;
  update();
  Q_EMIT valueChanged(value_);
}

void ProgressBase::setMinimum(int min) {
  minimum_ = min;
  if (maximum_ < minimum_) {
    maximum_ = minimum_;
  }
  setValue(value_);    // 重新做一次边界收敛
  updateTimerState();  // 可能进出忙碌态
}

void ProgressBase::setMaximum(int max) {
  maximum_ = max;
  if (minimum_ > maximum_) {
    minimum_ = maximum_;
  }
  setValue(value_);
  updateTimerState();
}

void ProgressBase::setRange(int min, int max) {
  minimum_ = min;
  maximum_ = qMax(min, max);
  setValue(value_);
  updateTimerState();
}

void ProgressBase::setTextVisible(bool visible) {
  if (text_visible_ == visible) {
    return;
  }
  text_visible_ = visible;
  update();
}

void ProgressBase::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  updateTimerState();
}

void ProgressBase::hideEvent(QHideEvent* event) {
  QWidget::hideEvent(event);
  updateTimerState();
}

void ProgressBase::updateTimerState() {
  if (isVisible() && isAnimated()) {
    if (!timer_->isActive()) {
      timer_->start();
    }
  } else {
    timer_->stop();
  }
}
