#include "hourglass_progress_bar.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

HourglassProgress::HourglassProgress(QWidget* parent) : ProgressBase(parent) {}

void HourglassProgress::setFrameColor(const QColor& c) {
  frameColor_ = c;
  update();
}

void HourglassProgress::setSandColor(const QColor& c) {
  sandColor_ = c;
  update();
}

void HourglassProgress::setFrameWidth(int w) {
  frameWidth_ = qMax(1, w);
  update();
}

void HourglassProgress::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.translate(rect().center());

  // 给外框线宽和文字留出余量
  const qreal r = (qMin(width(), height()) / 2.0) - frameWidth_ - 2.0;
  if (r <= 0) {
    return;
  }

  // 外框：两个共顶点 (0,0) 的三角形，路径连起来正好是沙漏轮廓
  QPainterPath frame;
  frame.moveTo(-r, -r);
  frame.lineTo(r, -r);
  frame.lineTo(0, 0);
  frame.lineTo(r, r);
  frame.lineTo(-r, r);
  frame.lineTo(0, 0);
  frame.closeSubpath();

  p.setPen(QPen(frameColor_, frameWidth_, Qt::SolidLine, Qt::RoundCap,
                Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  p.drawPath(frame);

  // 沙子
  p.setPen(Qt::NoPen);
  p.setBrush(sandColor_);

  const qreal t = ratio();

  // 上半个：剩余沙子面积正比于 1-t，故边长取平方根
  const qreal up = r * qSqrt(qMax(0.0, 1.0 - t));
  if (up > 0.5) {
    QPainterPath sand;
    sand.moveTo(-up, -up);
    sand.lineTo(up, -up);
    sand.lineTo(0, 0);
    sand.closeSubpath();
    p.drawPath(sand);
  }

  // 下半个：已落下的沙子面积正比于 t
  const qreal down = r * qSqrt(qMax(0.0, t));
  if (down > 0.5) {
    QPainterPath sand;
    sand.moveTo(-down, down);
    sand.lineTo(down, down);
    sand.lineTo(0, 0);
    sand.closeSubpath();
    p.drawPath(sand);
  }

  if (textVisible_) {
    QFont f = font();
    f.setPixelSize(qMax(10, static_cast<int>(r * 0.5)));
    f.setBold(true);
    p.setFont(f);
    p.setPen(sandColor_);
    p.drawText(QRectF(-r, -r, r * 2, r * 2), Qt::AlignCenter,
               QStringLiteral("%1%").arg(qRound(t * 100)));
  }
}
