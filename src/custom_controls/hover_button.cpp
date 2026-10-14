#include "hover_button.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>

HoverButton::HoverButton(const QString& text, Variant v, QWidget* parent)
    : QAbstractButton(parent), variant_(v) {
  setCursor(Qt::PointingHandCursor);
  setMinimumHeight(50);
  QAbstractButton::setText(text);
  setAttribute(Qt::WA_Hover, true);
  anim_ = new QPropertyAnimation(this, "hoverProgress", this);
  anim_->setDuration(220);
  anim_->setEasingCurve(QEasingCurve::OutCubic);
}

void HoverButton::setLeadingIcon(const QIcon& icon) {
  icon_ = icon;
  update();
}

void HoverButton::setAccentColor(const QColor& c) {
  accent_ = c;
  update();
}

void HoverButton::setPrimaryBackground(const QColor& c) {
  primary_bg_ = c;
  update();
}

void HoverButton::setPrimaryTextColor(const QColor& c) {
  primary_text_ = c;
  update();
}

void HoverButton::setSecondaryBackground(const QColor& c) {
  secondary_bg_ = c;
  update();
}

void HoverButton::setSecondaryBorder(const QColor& c) {
  secondary_border_ = c;
  update();
}

void HoverButton::setSecondaryTextColor(const QColor& c) {
  secondary_text_ = c;
  update();
}

void HoverButton::setHoverProgress(qreal p) {
  hp_ = p;
  update();
}

void HoverButton::enterEvent(QEvent*) {
  anim_->stop();
  anim_->setStartValue(hp_);
  anim_->setEndValue(1.0);
  anim_->start();
}

void HoverButton::leaveEvent(QEvent*) {
  anim_->stop();
  anim_->setStartValue(hp_);
  anim_->setEndValue(0.0);
  anim_->start();
}

void HoverButton::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);

  if (!isEnabled())
    p.setOpacity(0.55);

  const QRectF r = rect();
  const qreal radius = r.height() / 2.0;
  QPainterPath clip;
  clip.addRoundedRect(r.adjusted(0.75, 0.75, -0.75, -0.75), radius, radius);

  if (variant_ == Primary) {
    p.fillPath(clip, primary_bg_);
  } else {
    p.fillPath(clip, secondary_bg_);
    p.setPen(QPen(secondary_border_, 1.5));
    p.drawPath(clip);
  }

  if (hp_ > 0.001) {
    QColor ac = accent_;
    ac.setAlphaF(hp_);
    p.fillPath(clip, ac);
  }

  QFont f = font();
  f.setPointSizeF(11.5);
  f.setWeight(QFont::DemiBold);
  p.setFont(f);

  const QString label = text();  // 读基类，改名自动同步

  const QColor base_text_color =
      (variant_ == Primary) ? primary_text_ : secondary_text_;
  {
    QColor c = base_text_color;
    c.setAlphaF(qMax(0.0, 1.0 - hp_));
    p.setPen(c);

    QRectF tr = r.adjusted(hp_ * 40, 0, 0, 0);
    if (variant_ == Secondary && !icon_.isNull()) {
      QFontMetrics fm(f);
      qreal tw = fm.horizontalAdvance(label);
      qreal total = 18 + 10 + tw;
      qreal x0 = tr.center().x() - total / 2;
      QRectF icon_rect(x0, tr.center().y() - 9, 18, 18);
      icon_.paint(&p, icon_rect.toRect(), Qt::AlignCenter, QIcon::Normal);
      QRectF text_rect(x0 + 18 + 10, tr.top(), tw + 4, tr.height());
      p.drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft, label);
    } else {
      p.drawText(tr, Qt::AlignCenter, label);
    }
  }

  if (hp_ > 0.001) {
    QColor c = Qt::white;
    c.setAlphaF(hp_);
    p.setPen(c);

    QFontMetrics fm(f);
    qreal tw = fm.horizontalAdvance(label);
    qreal gap = 8;
    qreal arrow_w = 16;
    qreal total = tw + gap + arrow_w;
    qreal x0 = r.center().x() - total / 2;

    p.drawText(QRectF(x0, 0, tw + 2, r.height()),
               Qt::AlignVCenter | Qt::AlignLeft, label);

    p.save();
    p.translate(x0 + tw + gap + arrow_w / 2.0, r.center().y());
    QPen pen(c, 2.5);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.drawLine(QPointF(-8, 0), QPointF(6, 0));
    p.drawLine(QPointF(2, -4), QPointF(6, 0));
    p.drawLine(QPointF(2, 4), QPointF(6, 0));
    p.restore();
  }
}