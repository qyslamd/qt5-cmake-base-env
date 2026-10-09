#include "tour_overlay_mask.h"

#include <QEasingCurve>
#include <QPainter>
#include <QPainterPath>
#include <QVariant>
#include <QVariantAnimation>

#include "tour_colors.h"

OverlayMask::OverlayMask(QObject* parent) : QObject(parent) {
  anime_ = new QVariantAnimation(this);
  anime_->setDuration(kAnimeDur);
  anime_->setEasingCurve(QEasingCurve::InOutCubic);
  connect(anime_, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& value) {
            shown_anchor_ = value.value<QRect>();
            emit repaintRequested();
          });
}

void OverlayMask::setHighlight(const QRect& anchor, const QList<QRect>& extras,
                               bool animated) {
  highlight_.anchor = anchor;
  highlight_.extras = extras;

  // 多靶不走动画（设计 D2）：并集难看、N 路并行不值。**stop() 不能省**：
  // 上一步的动画还在跑时（kAnimeDur = 300 ms，连点两下「下一步」就会撞上）
  // 它会经 valueChanged 继续写 shown_anchor_，把下面那行赋值盖掉
  if (!animated || !extras.isEmpty()) {
    anime_->stop();
    shown_anchor_ = anchor;
  } else {
    // 从**此刻显示中的框**出发（动画停着时它就是上一处的锚点）
    anime_->setStartValue(shown_anchor_);
    anime_->setEndValue(anchor);
    anime_->start();
  }
  emit repaintRequested();
}

void OverlayMask::setPixmap(const QPixmap& pixmap) {
  pixmap_ = pixmap;
  emit repaintRequested();
}

void OverlayMask::clear() {
  // 一处清（原先是三个成员各写一句，漏一个就是"屏幕上留着没有依托的孤框"）
  highlight_ = Highlight();
  shown_anchor_ = QRect();
  pixmap_ = QPixmap();
  // 在途的动画别再把框写回来，否则清完之后下一帧又冒出一个孤框
  anime_->stop();
  emit repaintRequested();
}

bool OverlayMask::hitTarget(const QPoint& pos) const {
  for (const QRect& r : TargetRects()) {
    if (r.adjusted(-margin_, -margin_, margin_, margin_).contains(pos)) {
      return true;
    }
  }
  return false;
}

QList<QRect> OverlayMask::TargetRects() const {
  QList<QRect> rects;
  // 画的是**显示中的**框，不是 highlight_.anchor：动画跑着时两者不同
  // （后者是它要去的地方）
  if (!shown_anchor_.isNull()) {
    rects.append(shown_anchor_);
  }
  rects.append(highlight_.extras);
  return rects;
}

void OverlayMask::paint(QPainter* painter, const QRect& viewport) const {
  // 锚点 + 额外靶一起挖（《一步多靶设计》§3.2）。锚点可能为空（纯讲解步），
  // 那一步只画额外靶；一处都没有就整块盖住
  const QList<QRect> rects = TargetRects();
  if (rects.isEmpty()) {
    painter->fillRect(viewport, TourMaskColor());
    return;
  }

  QPainterPath full_path;
  full_path.addRect(viewport);

  QPainterPath holes;
  // **WindingFill 不能省**：路径的集合运算把路径当"区域"，而区域由 fill rule
  // 决定——默认的 OddEvenFill 下，两个重叠的圆角矩形在重叠区是"偶数次覆盖"、
  // 算作洞外，相减之后那块仍被遮罩涂黑。相邻两个框必然重叠（卡片的
  // QHBoxLayout 间距 8px，而框往外扩 margin_ = 10），不改就是一条条黑竖条
  holes.setFillRule(Qt::WindingFill);
  for (const QRect& r : rects) {
    holes.addRoundedRect(r.adjusted(-margin_, -margin_, margin_, margin_), 8,
                         8);
  }

  // 通过挖孔的方式突出高亮
  painter->fillPath(full_path - holes, TourMaskColor());

  // 绘制一个虚线外框（accent 色）
  painter->save();
  painter->setPen(QPen(TourAccentColor(), borderWidth_, Qt::DashLine,
                       Qt::RoundCap, Qt::RoundJoin));
  for (const QRect& r : rects) {
    painter->drawRoundedRect(r.adjusted(-margin_, -margin_, margin_, margin_),
                             8, 8);
  }
  painter->restore();

  // 如果有图片，在区域内绘制图片（图片只可能是锚点：多靶不支持图片，见 D4）
  if (!pixmap_.isNull() && !shown_anchor_.isNull()) {
    painter->drawPixmap(shown_anchor_, pixmap_, pixmap_.rect());
  }
}
