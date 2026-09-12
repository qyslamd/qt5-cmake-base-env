#include "dot_circle_progress_bar.h"

#include <QPainter>
#include <QPainterPath>

namespace {

// 忙碌绕圈的相位步长：每帧 0.012，约 83 帧（2.5 秒）转一圈
constexpr qreal kPhaseStep = 0.012;

}  // namespace

DotCircleProgress::DotCircleProgress(QWidget* parent) : ProgressBase(parent) {}

void DotCircleProgress::tick() {
  phase_ += kPhaseStep;
  if (phase_ > 1.0) {
    phase_ -= 1.0;
  }
}

void DotCircleProgress::setColor(const QColor& c) {
  color_ = c;
  update();
}

void DotCircleProgress::setTrackColor(const QColor& c) {
  track_color_ = c;
  update();
}

void DotCircleProgress::setDotRadius(int r) {
  dot_radius_ = qMax(1, r);
  update();
}

void DotCircleProgress::setTailCount(int n) {
  tail_count_ = qMax(0, n);
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
  p.setPen(QPen(track_color_, track_width_, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(rect, 90 * 16, -360 * 16);

  const bool busy = isBusy();
  // 忙碌态：亮点以相位为位置绕圈；进度态：亮点停在 ratio() 处
  const qreal t = busy ? phase_ : ratio();
  if (!busy && t <= 0.0) {
    return;
  }

  if (!busy) {
    // 已走过的浅色弧
    QColor arc_color = color_;
    arc_color.setAlpha(50);
    p.setPen(QPen(arc_color, track_width_, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(rect, 90 * 16, qRound(-360 * 16 * t));
  }

  // 拖尾 + 头部亮点：从尾画到头，保证头部压在最上层
  p.setPen(Qt::NoPen);
  for (int i = tail_count_; i >= 0; --i) {
    qreal pos = t - (i * tail_step_);
    if (busy) {
      // 绕圈时拖尾要跨过起点接到上一圈，否则每圈开头拖尾都会消失
      while (pos < 0.0) {
        pos += 1.0;
      }
    } else if (pos < 0.0) {
      continue;
    }
    const QPointF pt = path.pointAtPercent(pos);
    const qreal k = (tail_count_ == 0) ? 0.0 : qreal(i) / tail_count_;

    QColor c = color_;
    c.setAlpha(qRound(255 * (1.0 - (k * 0.85))));
    p.setBrush(c);
    p.drawEllipse(pt, dot_radius_ * (1.0 - (k * 0.55)),
                  dot_radius_ * (1.0 - (k * 0.55)));
  }

  if (text_visible_ && !busy) {
    QFont f = font();
    f.setPixelSize(qMax(10, static_cast<int>(side * 0.22)));
    f.setBold(true);
    p.setFont(f);
    p.setPen(color_);
    p.drawText(rect, Qt::AlignCenter,
               QStringLiteral("%1%").arg(qRound(t * 100)));
  }
}
