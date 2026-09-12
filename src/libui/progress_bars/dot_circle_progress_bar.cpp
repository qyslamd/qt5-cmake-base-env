#include "dot_circle_progress_bar.h"

#include <QPainter>
#include <QPainterPath>

DotCircleProgress::DotCircleProgress(QWidget* parent) : ProgressBase(parent) {}

void DotCircleProgress::setColor(const QColor& c) {
  color_ = c;
  update();
}

void DotCircleProgress::setTrackColor(const QColor& c) {
  trackColor_ = c;
  update();
}

void DotCircleProgress::setDotRadius(int r) {
  dotRadius_ = qMax(1, r);
  update();
}

void DotCircleProgress::setTailCount(int n) {
  tailCount_ = qMax(0, n);
  update();
}

void DotCircleProgress::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // 圆环矩形：居中并留白，取短边保证是正圆
  const qreal margin = qMin(width(), height()) * 0.15;
  QRectF rect = QRectF(this->rect()).adjusted(margin, margin, -margin, -margin);
  const qreal side = qMin(rect.width(), rect.height());
  rect.setWidth(side);
  rect.setHeight(side);
  rect.moveCenter(QPointF(this->rect().center()));

  // 等长圆路径，起点在 12 点方向、顺时针一周，供 pointAtPercent 取点
  QPainterPath path;
  path.moveTo(rect.center().x(), rect.top());
  path.arcTo(rect, 90, -360);

  // 底轨
  p.setPen(QPen(trackColor_, trackWidth_, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(rect, 90 * 16, -360 * 16);

  const qreal t = ratio();
  if (t <= 0.0) {
    return;
  }

  // 已走过的浅色弧
  QColor arc_color = color_;
  arc_color.setAlpha(50);
  p.setPen(QPen(arc_color, trackWidth_, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(rect, 90 * 16, qRound(-360 * 16 * t));

  // 拖尾 + 头部亮点：从尾画到头，保证头部压在最上层
  p.setPen(Qt::NoPen);
  for (int i = tailCount_; i >= 0; --i) {
    const qreal pos = t - (i * tailStep_);
    if (pos < 0.0) {
      continue;
    }
    const QPointF pt = path.pointAtPercent(pos);
    const qreal k = (tailCount_ == 0) ? 0.0 : qreal(i) / tailCount_;

    QColor c = color_;
    c.setAlpha(qRound(255 * (1.0 - (k * 0.85))));
    p.setBrush(c);
    p.drawEllipse(pt, dotRadius_ * (1.0 - (k * 0.55)),
                  dotRadius_ * (1.0 - (k * 0.55)));
  }

  if (textVisible_) {
    QFont f = font();
    f.setPixelSize(qMax(10, static_cast<int>(side * 0.22)));
    f.setBold(true);
    p.setFont(f);
    p.setPen(color_);
    p.drawText(rect, Qt::AlignCenter,
               QStringLiteral("%1%").arg(qRound(t * 100)));
  }
}
