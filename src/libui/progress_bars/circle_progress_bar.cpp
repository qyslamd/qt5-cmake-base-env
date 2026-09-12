#include "circle_progress_bar.h"

#include <QPainter>

CircleProgress::CircleProgress(QWidget* parent) : ProgressBase(parent) {}

void CircleProgress::setLineWidth(int w) {
  lineWidth_ = qMax(1, w);
  update();
}

void CircleProgress::setColor(const QColor& c) {
  color_ = c;
  update();
}
void CircleProgress::setBackgroundColor(const QColor& c) {
  backgroundColor_ = c;
  update();
}
void CircleProgress::setTextColor(const QColor& c) {
  textColor_ = c;
  update();
}

void CircleProgress::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const qreal side = qMin(width(), height());
  const qreal penW = lineWidth_;
  const qreal half = penW / 2.0;

  QRectF rect((width() - side) / 2.0 + half, (height() - side) / 2.0 + half,
              side - penW, side - penW);

  // 底轨
  p.setPen(QPen(backgroundColor_, penW, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(rect, startAngle_ * 16, -spanAngle_ * 16);

  // 进度弧
  const int span = qRound(-spanAngle_ * 16 * ratio());
  if (span != 0) {
    p.setPen(QPen(color_, penW, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(rect, startAngle_ * 16, span);
  }

  // 文字
  if (textVisible_) {
    QFont f = font();
    f.setPixelSize(qMax(10, static_cast<int>(side * 0.18)));
    f.setBold(true);
    p.setFont(f);
    p.setPen(textColor_);
    p.drawText(rect, Qt::AlignCenter,
               QStringLiteral("%1%").arg(qRound(ratio() * 100)));
  }
}