#include "sector_menu.h"

#include <cmath>

#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QtMath>

namespace {

// 默认扇区底色与文字色
const QColor kDefaultBackground(128, 128, 128, 50);
const QColor kDefaultTextColor(Qt::white);

// 悬停时叠加的半透明白，用于提亮当前扇区
const QColor kHoverOverlay(255, 255, 255, 60);

// 标题字号占环宽的比例，以及标题允许占用的中弧宽度比例
constexpr qreal kFontHeightRatio = 0.18;
constexpr qreal kLabelWidthRatio = 0.9;

// 圆周上一点。注意 y 取反：Qt 的角度以逆时针为正、以数学坐标系的
// y 轴向上为准，而控件坐标 y 轴向下，两者需换算。
QPointF PointOnCircle(const QPointF& center, qreal radius, qreal degrees) {
  const qreal radians = qDegreesToRadians(degrees);
  return {center.x() + (radius * std::cos(radians)),
          center.y() - (radius * std::sin(radians))};
}

}  // namespace

SectorMenu::SectorMenu(QWidget* parent) : QWidget(parent) { initUi(); }

void SectorMenu::initUi() {
  // 悬停与点击需要接收鼠标移动事件，即使未按下按键
  setAttribute(Qt::WA_Hover, true);
  setMouseTracking(true);
  setMinimumSize(160, 160);
}

void SectorMenu::addItem(const QString& title,
                         std::function<void()> on_clicked,
                         const QColor& background, const QColor& text_color) {
  Item item;
  item.title = title;
  item.on_clicked = std::move(on_clicked);
  item.background = background.isValid() ? background : kDefaultBackground;
  item.text_color = text_color.isValid() ? text_color : kDefaultTextColor;
  items_.append(item);
  update();
}

void SectorMenu::setRadiusRatio(qreal outer_ratio, qreal inner_ratio) {
  // 夹到合法区间：内外比须落在 (0,1] 且外比大于内比，否则扇区会退化
  outer_ratio_ = qBound(0.05, outer_ratio, 1.0);
  inner_ratio_ = qBound(0.0, inner_ratio, outer_ratio_ - 0.05);
  update();
}

void SectorMenu::setStartAngle(qreal degrees) {
  start_angle_ = degrees;
  update();
}

QPainterPath SectorMenu::sectorPath(int index) const {
  const QPointF center(width() / 2.0, height() / 2.0);
  const qreal max_radius = qMin(width(), height()) / 2.0;
  const qreal outer_radius = max_radius * outer_ratio_;
  const qreal inner_radius = max_radius * inner_ratio_;

  const qreal span = 360.0 / items_.size();
  const qreal begin = start_angle_ + (span * index);

  const QRectF outer_rect(center.x() - outer_radius, center.y() - outer_radius,
                          outer_radius * 2.0, outer_radius * 2.0);
  const QRectF inner_rect(center.x() - inner_radius, center.y() - inner_radius,
                          inner_radius * 2.0, inner_radius * 2.0);

  // 外弧沿 begin -> begin+span 正向画，内弧再反向画回来，闭合即得扇环
  QPainterPath path;
  path.moveTo(PointOnCircle(center, outer_radius, begin));
  path.arcTo(outer_rect, begin, span);
  path.lineTo(PointOnCircle(center, inner_radius, begin + span));
  path.arcTo(inner_rect, begin + span, -span);
  path.closeSubpath();
  return path;
}

void SectorMenu::paintEvent(QPaintEvent* /*event*/) {
  if (items_.isEmpty()) {
    return;
  }

  QPainter painter(this);
  painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

  drawItems(&painter);
}

void SectorMenu::drawItems(QPainter* painter) {
  for (int i = 0; i < items_.size(); ++i) {
    const QPainterPath path = sectorPath(i);
    painter->fillPath(path, items_[i].background);
    if (items_[i].hover) {
      painter->fillPath(path, kHoverOverlay);
    }
  }

  // 文字单独一轮绘制：避免后一个扇区的底色盖住前一个扇区的标题
  for (int i = 0; i < items_.size(); ++i) {
    drawLabel(painter, items_[i], i);
  }
}

void SectorMenu::drawLabel(QPainter* painter, const Item& item, int index) {
  if (item.title.isEmpty()) {
    return;
  }

  const QPointF center(width() / 2.0, height() / 2.0);
  const qreal max_radius = qMin(width(), height()) / 2.0;
  const qreal outer_radius = max_radius * outer_ratio_;
  const qreal inner_radius = max_radius * inner_ratio_;

  const qreal span = 360.0 / items_.size();
  const qreal center_degrees = start_angle_ + (span * (index + 0.5));
  // 标题放在环宽靠外的位置，内圈留给图标之类的元素
  const qreal text_radius = (outer_radius + (outer_radius + inner_radius) / 2.0)
                            / 2.0;

  painter->save();
  // 把坐标系转到扇区中心方向，此后按普通横排放置文字即可保持正立
  painter->translate(center);
  painter->rotate(90.0 - center_degrees);

  const qreal ring_width = outer_radius - inner_radius;
  QFont font = painter->font();
  font.setPixelSize(qMax(qreal(1.0), ring_width * kFontHeightRatio));
  font.setBold(true);

  // 标题过宽时按比例缩小，保证不越出扇区
  const qreal max_width = ring_width * kLabelWidthRatio;
  QFontMetrics metrics(font);
  const qreal width = metrics.boundingRect(item.title).width();
  if (width > max_width && width > 0.0) {
    font.setPixelSize(qMax(qreal(1.0), font.pixelSize() * max_width / width));
  }

  painter->setFont(font);
  painter->setPen(item.text_color);

  QRectF text_rect = painter->fontMetrics().boundingRect(item.title);
  text_rect.moveCenter(QPointF(0.0, -text_radius));
  painter->drawText(text_rect, Qt::AlignCenter, item.title);

  painter->restore();
}

int SectorMenu::itemAt(const QPoint& pos) const {
  if (items_.isEmpty()) {
    return -1;
  }

  const QPointF center(width() / 2.0, height() / 2.0);
  const qreal dx = pos.x() - center.x();
  const qreal dy = center.y() - pos.y();  // 取反以匹配角度约定
  const qreal radius = std::sqrt((dx * dx) + (dy * dy));

  const qreal max_radius = qMin(width(), height()) / 2.0;
  if (radius < max_radius * inner_ratio_ || radius > max_radius * outer_ratio_) {
    return -1;
  }

  // 换算成相对起始角的偏移，归一化到 [0, 360) 后按扇区跨度取整
  qreal offset = qRadiansToDegrees(std::atan2(dy, dx)) - start_angle_;
  while (offset < 0.0) {
    offset += 360.0;
  }
  while (offset >= 360.0) {
    offset -= 360.0;
  }

  const int index =
      static_cast<int>(offset / (360.0 / items_.size()));
  return qBound(0, index, items_.size() - 1);
}

void SectorMenu::mouseMoveEvent(QMouseEvent* event) {
  const int index = itemAt(event->pos());
  if (index != hover_index_) {
    if (hover_index_ >= 0 && hover_index_ < items_.size()) {
      items_[hover_index_].hover = false;
    }
    hover_index_ = index;
    if (hover_index_ >= 0) {
      items_[hover_index_].hover = true;
    }
    update();
  }
  QWidget::mouseMoveEvent(event);
}

void SectorMenu::mouseReleaseEvent(QMouseEvent* event) {
  const int index = itemAt(event->pos());
  if (index >= 0 && items_[index].on_clicked) {
    items_[index].on_clicked();
  }
  QWidget::mouseReleaseEvent(event);
}

void SectorMenu::leaveEvent(QEvent* event) {
  if (hover_index_ >= 0 && hover_index_ < items_.size()) {
    items_[hover_index_].hover = false;
  }
  hover_index_ = -1;
  update();
  QWidget::leaveEvent(event);
}
