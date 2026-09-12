#include "hourglass_progress_bar.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {

// 忙碌循环的时间分配，取整个循环的比例：漏空 -> 翻转 -> 反向漏空 -> 翻回。
// 配合相位步长，一段漏空约 2 秒、一段翻转约 0.5 秒
constexpr qreal kDrainSpan = 0.4;
constexpr qreal kFlipSpan = 0.1;
constexpr qreal kHalfTurn = 180.0;

// 相位步长：每帧 0.006，约 167 帧（5 秒）走完一个循环
constexpr qreal kPhaseStep = 0.006;

// 忙碌循环在某一相位上的样子：drained 是已漏掉的比例（0 表示沙全在
// 上端），rotation 是玻璃的旋转角。两端与循环交界处都连续，不跳变
struct BusyCycle {
  qreal drained;
  qreal rotation;
};

BusyCycle BusyAt(qreal phase) {
  if (phase < kDrainSpan) {
    return {phase / kDrainSpan, 0.0};
  }
  if (phase < kDrainSpan + kFlipSpan) {
    return {1.0, kHalfTurn * (phase - kDrainSpan) / kFlipSpan};
  }
  if (phase < (kDrainSpan * 2.0) + kFlipSpan) {
    // 翻过来之后沙全在（玻璃自身的）下端，反向漏回上端
    return {1.0 - ((phase - kDrainSpan - kFlipSpan) / kDrainSpan), kHalfTurn};
  }
  // 最后一段翻回原位，方向与上一段相反：绕回时角度正好归零，
  // 不必依赖 360 度与 0 度等价
  const qreal u = (phase - ((kDrainSpan * 2.0) + kFlipSpan)) / kFlipSpan;
  return {0.0, kHalfTurn * (1.0 - u)};
}

}  // namespace

HourglassProgress::HourglassProgress(QWidget* parent) : ProgressBase(parent) {}

void HourglassProgress::tick() {
  phase_ += kPhaseStep;
  if (phase_ > 1.0) {
    phase_ -= 1.0;
  }
}

void HourglassProgress::setFrameColor(const QColor& c) {
  frame_color_ = c;
  update();
}

void HourglassProgress::setSandColor(const QColor& c) {
  sand_color_ = c;
  update();
}

void HourglassProgress::setFrameWidth(int w) {
  frame_width_ = qMax(1, w);
  update();
}

void HourglassProgress::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.translate(rect().center());

  // 给外框线宽和文字留出余量
  const qreal r = (qMin(width(), height()) / 2.0) - frame_width_ - 2.0;
  if (r <= 0) {
    return;
  }

  // 进度态：已漏掉的比例就是 ratio()；忙碌态：由相位算出已漏比例与旋转角，
  // 翻转时沙子随玻璃一起转，所以"满"的那端会翻上来接着漏。旋转只作用在
  // 玻璃与沙子上，忙碌态本来就不画文字
  const bool busy = isBusy();
  qreal t = ratio();
  if (busy) {
    const BusyCycle cycle = BusyAt(phase_);
    t = cycle.drained;
    p.rotate(cycle.rotation);
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

  p.setPen(QPen(frame_color_, frame_width_, Qt::SolidLine, Qt::RoundCap,
                Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  p.drawPath(frame);

  // 沙子
  p.setPen(Qt::NoPen);
  p.setBrush(sand_color_);

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

  if (text_visible_ && !busy) {
    QFont f = font();
    f.setPixelSize(qMax(10, static_cast<int>(r * 0.5)));
    f.setBold(true);
    p.setFont(f);
    p.setPen(sand_color_);
    p.drawText(QRectF(-r, -r, r * 2, r * 2), Qt::AlignCenter,
               QStringLiteral("%1%").arg(qRound(t * 100)));
  }
}
