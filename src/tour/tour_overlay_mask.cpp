#include "tour_overlay_mask.h"

#include <QEasingCurve>
#include <QPainter>
#include <QPainterPath>
#include <QVariant>
#include <QVariantAnimation>

#include "tour_colors.h"

TourOverlayMask::TourOverlayMask(QObject* parent) : QObject(parent) {
  anime_ = new QVariantAnimation(this);
  anime_->setDuration(kAnimeDur);
  anime_->setEasingCurve(QEasingCurve::InOutCubic);
  connect(anime_, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& value) {
            shown_anchor_ = value.value<QRect>();
            emit repaintRequested();
          });
}

void TourOverlayMask::setHighlight(const QRect& anchor,
                                   const QList<QRect>& extras, bool animated) {
  highlight_.anchor = anchor;
  highlight_.extras = extras;

  // 多靶不走动画（设计 D2）。**stop() 不能省**：上一步的动画还在跑时它会继续
  // 写 shown_anchor_，把下面那行赋值盖掉
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

void TourOverlayMask::setPixmap(const QPixmap& pixmap) {
  pixmap_ = pixmap;
  emit repaintRequested();
}

void TourOverlayMask::clear() {
  // 一处清：三个成员分着清，漏一个屏幕上就留着没有依托的孤框
  highlight_ = Highlight();
  shown_anchor_ = QRect();
  pixmap_ = QPixmap();
  // 在途的动画别再把框写回来
  anime_->stop();
  emit repaintRequested();
}

bool TourOverlayMask::hitTarget(const QPoint& pos) const {
  for (const QRect& r : TargetRects()) {
    if (r.adjusted(-kMargin, -kMargin, kMargin, kMargin).contains(pos)) {
      return true;
    }
  }
  return false;
}

QList<QRect> TourOverlayMask::TargetRects() const {
  QList<QRect> rects;
  // 画的是**显示中的**框，不是 highlight_.anchor（后者是它要去的地方）
  if (!shown_anchor_.isNull()) {
    rects.append(shown_anchor_);
  }
  rects.append(highlight_.extras);
  return rects;
}

void TourOverlayMask::paint(QPainter* painter, const QRect& viewport) const {
  // 锚点 + 额外靶一起挖（设计 §3.2）。一处都没有就整块盖住
  const QList<QRect> rects = TargetRects();
  if (rects.isEmpty()) {
    painter->fillRect(viewport, TourMaskColor());
    return;
  }

  QPainterPath full_path;
  full_path.addRect(viewport);

  QPainterPath holes;
  // **WindingFill 不能省**：默认的 OddEvenFill 下，两个重叠的洞在重叠区算作
  // "洞外"，相减之后那块仍被涂黑——相邻的框必然重叠（间距 8px < 外扩 10px）
  holes.setFillRule(Qt::WindingFill);
  for (const QRect& r : rects) {
    holes.addRoundedRect(r.adjusted(-kMargin, -kMargin, kMargin, kMargin), 8,
                         8);
  }

  painter->fillPath(full_path - holes, TourMaskColor());

  painter->save();
  painter->setPen(QPen(TourAccentColor(), kBorderWidth, Qt::DashLine,
                       Qt::RoundCap, Qt::RoundJoin));
  for (const QRect& r : rects) {
    painter->drawRoundedRect(r.adjusted(-kMargin, -kMargin, kMargin, kMargin),
                             8, 8);
  }
  painter->restore();

  // 图只可能是锚点的：多靶不支持图片（见 D4）
  if (!pixmap_.isNull() && !shown_anchor_.isNull()) {
    painter->drawPixmap(shown_anchor_, pixmap_, pixmap_.rect());
  }
}
