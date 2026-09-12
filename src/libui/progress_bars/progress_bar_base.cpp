#include "progress_bar_base.h"

ProgressBase::ProgressBase(QWidget* parent) : QWidget(parent) {
  setMinimumSize(80, 80);
}

ProgressBase::~ProgressBase() = default;

qreal ProgressBase::ratio() const {
  if (maximum_ <= minimum_) {
    return 0.0;
  }
  const qreal r = qreal(value_ - minimum_) / qreal(maximum_ - minimum_);
  return qBound(0.0, r, 1.0);
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
  setValue(value_);  // 重新做一次边界收敛
}

void ProgressBase::setMaximum(int max) {
  maximum_ = max;
  if (minimum_ > maximum_) {
    minimum_ = maximum_;
  }
  setValue(value_);
}

void ProgressBase::setRange(int min, int max) {
  minimum_ = min;
  maximum_ = qMax(min, max);
  setValue(value_);
}

void ProgressBase::setTextVisible(bool visible) {
  if (text_visible_ == visible) {
    return;
  }
  text_visible_ = visible;
  update();
}