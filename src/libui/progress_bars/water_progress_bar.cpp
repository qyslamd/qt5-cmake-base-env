#include "water_progress_bar.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QtMath>

namespace {

// 波浪相位的推进步长与帧间隔
constexpr qreal kPhaseStep = 0.09;
constexpr int kWaveIntervalMs = 30;

// 水面起伏的采样点数，以及第二层波的振幅比例与频率倍数
constexpr int kWaveSteps = 80;
constexpr qreal kSecondWaveRatio = 0.6;
constexpr qreal kFirstWaveFrequency = 1.5;
constexpr qreal kSecondWaveFrequency = 3.0;

}  // namespace

WaterProgress::WaterProgress(QWidget* parent) : ProgressBase(parent) {
  timer_ = new QTimer(this);
  timer_->setInterval(kWaveIntervalMs);
  connect(timer_, &QTimer::timeout, this, [this]() {
    phase_ += kPhaseStep;
    if (phase_ > 2.0 * M_PI) {
      phase_ -= 2.0 * M_PI;
    }
    update();
  });
  timer_->start();
}

void WaterProgress::setWaterColor(const QColor& c) {
  waterColor_ = c;
  update();
}

void WaterProgress::setBorderColor(const QColor& c) {
  borderColor_ = c;
  update();
}

void WaterProgress::setBorderWidth(int w) {
  borderWidth_ = qMax(0, w);
  update();
}

void WaterProgress::setWaveHeight(qreal h) {
  waveHeight_ = qMax(0.0, h);
  update();
}

void WaterProgress::startWave() { timer_->start(); }

void WaterProgress::stopWave() { timer_->stop(); }

void WaterProgress::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const qreal side = qMin(width(), height());
  QRectF outer((width() - side) / 2.0, (height() - side) / 2.0, side, side);
  QRectF inner =
      outer.adjusted(borderWidth_, borderWidth_, -borderWidth_, -borderWidth_);
  if (inner.width() <= 1 || inner.height() <= 1) {
    return;
  }

  p.save();
  QPainterPath clip;
  clip.addEllipse(inner);
  p.setClipPath(clip);

  // 水位线：ratio 为 0 时水面在底部，为 1 时到顶部
  const qreal water_top = inner.top() + (inner.height() * (1.0 - ratio()));

  // 双正弦叠加，画一个封闭水体。两端也取样，保证水体贴住圆的两侧
  const qreal left = inner.left();
  const qreal width = inner.width();
  const qreal amp1 = waveHeight_;
  const qreal amp2 = waveHeight_ * kSecondWaveRatio;

  QPainterPath wave;
  wave.moveTo(left, water_top);
  for (int i = 0; i <= kWaveSteps; ++i) {
    const qreal x = left + (width * i / kWaveSteps);
    const qreal y =
        water_top +
        (amp1 *
         qSin((2.0 * M_PI * (i * kFirstWaveFrequency / kWaveSteps)) + phase_)) +
        (amp2 * qSin((2.0 * M_PI * (i * kSecondWaveFrequency / kWaveSteps)) +
                     (phase_ * 1.7)));
    wave.lineTo(x, y);
  }
  wave.lineTo(inner.right(), inner.bottom());
  wave.lineTo(left, inner.bottom());
  wave.closeSubpath();

  QColor top_color = waterColor_;
  top_color.setAlpha(200);
  QColor bottom_color = waterColor_;
  bottom_color.setAlpha(120);
  QLinearGradient grad(0, inner.top(), 0, inner.bottom());
  grad.setColorAt(0.0, top_color);
  grad.setColorAt(1.0, bottom_color);

  p.setPen(Qt::NoPen);
  p.setBrush(grad);
  p.drawPath(wave);

  p.restore();

  // 圆形外框：内外两层，做出立体感
  p.setBrush(Qt::NoBrush);

  QColor outer_border = borderColor_;
  outer_border.setAlpha(180);
  p.setPen(QPen(outer_border, borderWidth_));
  p.drawEllipse(outer.adjusted(borderWidth_ / 2.0, borderWidth_ / 2.0,
                               -borderWidth_ / 2.0, -borderWidth_ / 2.0));

  const QColor inner_border(234, 242, 255, 140);
  p.setPen(QPen(inner_border, borderWidth_ * 0.6));
  p.drawEllipse(
      inner.adjusted(-borderWidth_, -borderWidth_, borderWidth_, borderWidth_));

  if (textVisible_) {
    QFont f = font();
    f.setPixelSize(qMax(10, static_cast<int>(side * 0.20)));
    f.setBold(true);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(outer, Qt::AlignCenter,
               QStringLiteral("%1%").arg(qRound(ratio() * 100)));
  }
}
