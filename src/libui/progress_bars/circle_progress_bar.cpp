#include "circle_progress_bar.h"

#include <QPainter>

CircleProgress::CircleProgress(QWidget* parent) : ProgressBase(parent) {}

void CircleProgress::setLineWidth(int w) {
  line_width_ = qMax(1, w);
  update();
}

void CircleProgress::setColor(const QColor& c) {
  color_ = c;
  update();
}
void CircleProgress::setBackgroundColor(const QColor& c) {
  background_color_ = c;
  update();
}
void CircleProgress::setTextColor(const QColor& c) {
  text_color_ = c;
  update();
}

void CircleProgress::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const qreal side = qMin(width(), height());
  const qreal pen_w = line_width_;
  const qreal half = pen_w / 2.0;

  QRectF rect((width() - side) / 2.0 + half, (height() - side) / 2.0 + half,
              side - pen_w, side - pen_w);

  // 底轨
  p.setPen(QPen(background_color_, pen_w, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(rect, start_angle_ * 16, -span_angle_ * 16);

  // 进度弧
  const int span = qRound(-span_angle_ * 16 * ratio());
  if (span != 0) {
    p.setPen(QPen(color_, pen_w, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(rect, start_angle_ * 16, span);
  }

  // 文字
  if (text_visible_) {
    QFont f = font();
    f.setPixelSize(qMax(10, static_cast<int>(side * 0.18)));
    f.setBold(true);
    p.setFont(f);
    p.setPen(text_color_);
    p.drawText(rect, Qt::AlignCenter,
               QStringLiteral("%1%").arg(qRound(ratio() * 100)));
  }
}