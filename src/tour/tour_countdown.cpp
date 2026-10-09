#include "tour_countdown.h"

namespace {

// 自动挡的时长与 tick 间隔；总 tick 数由这两个推出来
constexpr int kCountDownSeconds = 5;
constexpr int kCountDownTickMs = 30;
constexpr int kCountDownMaxTicks = kCountDownSeconds * 1000 / kCountDownTickMs;

}  // namespace

TourCountdown::TourCountdown(QObject* parent)
    : QObject(parent), max_ticks_(kCountDownMaxTicks) {
  timer_.setInterval(kCountDownTickMs);
  connect(&timer_, &QTimer::timeout, this, &TourCountdown::onTick);
}

void TourCountdown::setAutoMode(bool auto_mode) {
  auto_mode_ = auto_mode;
  tick_ = 0;
  max_ticks_ = kCountDownMaxTicks;
  silent_ = false;
  if (!auto_mode_) {
    timer_.stop();
  }
  emit changed();
}

void TourCountdown::startAuto() {
  if (!auto_mode_) {
    return;
  }
  max_ticks_ = kCountDownMaxTicks;
  // 起表这一帧就得把状态刷出去，否则第一帧还是上一次留下的空白
  time_percent_ = 0.0;
  tick_ = 0;
  remaining_ = kCountDownSeconds;
  emit changed();
  timer_.start();
}

void TourCountdown::startSilent(int seconds) {
  if (seconds <= 0) {
    return;
  }
  silent_ = true;
  max_ticks_ = seconds * 1000 / kCountDownTickMs;
  time_percent_ = 0.0;
  tick_ = 0;
  remaining_ = seconds;
  emit changed();
  timer_.start();
}

void TourCountdown::stop() { timer_.stop(); }

void TourCountdown::onTick() {
  // 走满本次该走的 tick 数就到点
  if (++tick_ >= max_ticks_) {
    time_percent_ = 1.0;
    timer_.stop();
    tick_ = 0;  // 复位，避免下次续接残留值
    const bool was_silent = silent_;
    // 静默那趟走到头：只收显示，不推进
    silent_ = false;
    emit changed();
    if (!was_silent) {
      emit finished();
    }
    return;
  }

  time_percent_ = static_cast<qreal>(tick_) / static_cast<qreal>(max_ticks_);

  // 剩余秒数由"已过去的比例"反推
  const int total_seconds = max_ticks_ * kCountDownTickMs / 1000;
  const qreal remain = 1.0 - time_percent_;
  remaining_ = static_cast<int>(total_seconds * remain);
  emit changed();
}
