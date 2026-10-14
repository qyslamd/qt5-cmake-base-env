#include "static_character_animator.h"

#include <cmath>

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {

QPainterPath topRoundedPath(const QRectF& r, qreal tl, qreal tr) {
  QPainterPath path;
  path.moveTo(r.left() + tl, r.top());
  path.lineTo(r.right() - tr, r.top());
  path.arcTo(QRectF(r.right() - tr * 2, r.top(), tr * 2, tr * 2), 90, -90);
  path.lineTo(r.right(), r.bottom());
  path.lineTo(r.left(), r.bottom());
  path.lineTo(r.left(), r.top() + tl);
  path.arcTo(QRectF(r.left(), r.top(), tl * 2, tl * 2), 180, -90);
  path.closeSubpath();
  return path;
}

QPainterPath halfCirclePath(const QRectF& r) {
  QPainterPath path;
  path.moveTo(r.left(), r.bottom());
  path.lineTo(r.left(), r.top() + r.width() / 2);
  QRectF arc_rect(r.left(), r.top(), r.width(), r.width());
  path.arcTo(arc_rect, 180, -180);
  path.lineTo(r.right(), r.bottom());
  path.closeSubpath();
  return path;
}

}  // namespace

StaticCharacterAnimator::StaticCharacterAnimator(QObject* parent)
    : CharacterAnimator(parent) {
  initChars();
}

void StaticCharacterAnimator::initChars() {
  chars_.clear();

  // 与 DefaultCharacterAnimator 同几何，同姿态
  {
    StaticChar c;
    c.body = QColor("#6c3ff5");
    c.base = QRectF(60, 360 - 370, 170, 370);
    c.corner = 10;
    c.eye_anchor = QPointF(60, 50);
    c.eye_size = 18;
    c.eye_gap = 28;
    c.pupil_ratio = 0.39;
    chars_.append(c);
  }
  {
    StaticChar c;
    c.body = QColor("#2d2d2d");
    c.base = QRectF(220, 360 - 290, 115, 290);
    c.corner = 8;
    c.eye_anchor = QPointF(41, 42);
    c.eye_size = 16;
    c.eye_gap = 20;
    c.pupil_ratio = 0.375;
    chars_.append(c);
  }
  {
    StaticChar c;
    c.body = QColor("#ff9b6b");
    c.base = QRectF(0, 360 - 190, 230, 190);
    c.half_circle = true;
    c.eye_anchor = QPointF(97, 100);
    c.eye_size = 12;
    c.eye_gap = 28;
    c.has_eyeball = false;
    c.has_mouth = true;
    c.mouth_anchor = QPointF(90, 120);
    c.mouth_w = 28;
    c.mouth_h = 14;
    chars_.append(c);
  }
  {
    StaticChar c;
    c.body = QColor("#e8d754");
    c.base = QRectF(290, 360 - 215, 135, 215);
    c.half_circle = true;
    c.eye_anchor = QPointF(67, 50);
    c.eye_size = 12;
    c.eye_gap = 20;
    c.has_eyeball = false;
    c.has_mouth = true;
    c.mouth_anchor = QPointF(55, 98);
    c.mouth_w = 50;
    c.mouth_h = 4;
    chars_.append(c);
  }
}

void StaticCharacterAnimator::updateScene(const QSize& panel_size) {
  const qreal w = panel_size.width();
  const qreal h = panel_size.height();
  scene_ = QRectF((w - 480.0) / 2.0, (h - 360.0) / 2.0 - 10, 480.0, 360.0);
}

void StaticCharacterAnimator::setInput(const Input& in) {
  if (in.panel_size != input_.panel_size) {
    updateScene(in.panel_size);
  }
  input_ = in;
  // 其余字段刻意不读：静态版不跟随鼠标、不响应打字/焦点/可见性
}

void StaticCharacterAnimator::paint(QPainter& p, const QRectF& rect) const {
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);

  drawBackground(p, rect);

  p.save();
  p.translate(scene_.topLeft());
  for (const StaticChar& c : chars_) {
    drawChar(p, c);
  }
  p.restore();

  drawLogo(p);
  // drawFooter(p, rect);
}

void StaticCharacterAnimator::drawBackground(QPainter& p,
                                             const QRectF& rect) const {
  QLinearGradient g(0, 0, rect.width(), rect.height());
  g.setColorAt(0.0, QColor("#d4d0dc"));
  g.setColorAt(0.5, QColor("#c8c4d0"));
  g.setColorAt(1.0, QColor("#bbb7c5"));
  p.fillRect(rect, g);

  {
    const QPointF c(rect.width() * 0.85, rect.height() * 0.25);
    QRadialGradient rg(c, 180);
    rg.setColorAt(0.0, QColor(180, 170, 200, 90));
    rg.setColorAt(1.0, QColor(180, 170, 200, 0));
    p.fillRect(rect, rg);
  }
  {
    const QPointF c(rect.width() * 0.15, rect.height() * 0.85);
    QRadialGradient rg(c, 220);
    rg.setColorAt(0.0, QColor(200, 195, 210, 70));
    rg.setColorAt(1.0, QColor(200, 195, 210, 0));
    p.fillRect(rect, rg);
  }
}

void StaticCharacterAnimator::drawLogo(QPainter& p) const {
  const QRectF icon(48, 40, 28, 28);

  QPainterPath star;
  const QPointF c = icon.center();
  const qreal R = 10, r = 4;
  for (int i = 0; i < 8; ++i) {
    const qreal a = M_PI / 4 * i - M_PI / 2;
    const qreal rr = (i % 2 == 0) ? R : r;
    const QPointF pt(c.x() + std::cos(a) * rr, c.y() + std::sin(a) * rr);
    if (i == 0) {
      star.moveTo(pt);
    } else {
      star.lineTo(pt);
    }
  }
  star.closeSubpath();

  p.save();
  p.setBrush(QColor(255, 255, 255, 60));
  p.setPen(Qt::NoPen);
  p.drawRoundedRect(icon, 6, 6);
  p.restore();

  p.setPen(QPen(Qt::white, 1.8));
  p.setBrush(Qt::NoBrush);
  p.drawPath(star);

  QFont f = p.font();
  f.setPointSizeF(11.5);
  f.setWeight(QFont::DemiBold);
  p.setFont(f);
  p.setPen(Qt::white);
  p.drawText(QRectF(84, 40, 240, 28), Qt::AlignVCenter | Qt::AlignLeft,
             logo_text_);
}

void StaticCharacterAnimator::drawFooter(QPainter& p,
                                         const QRectF& rect) const {
  QFont f = p.font();
  f.setPointSizeF(9.5);
  p.setFont(f);
  p.setPen(QColor(80, 70, 90, 180));

  qreal x = 48;
  const qreal y = rect.height() - 40;
  QFontMetrics fm(f);
  for (const QString& s : footer_links_) {
    const qreal w = fm.horizontalAdvance(s);
    p.drawText(QRectF(x, y, w + 4, 18), Qt::AlignVCenter | Qt::AlignLeft, s);
    x += w + 28;
  }
}

void StaticCharacterAnimator::drawChar(QPainter& p, const StaticChar& c) const {
  const QRectF body = c.base;

  p.save();
  p.setPen(Qt::NoPen);
  p.setBrush(c.body);
  if (c.half_circle) {
    p.drawPath(halfCirclePath(body));
  } else {
    p.drawPath(topRoundedPath(body, c.corner, c.corner));
  }

  // 瞳孔固定在中央
  const QPointF eye_origin = body.topLeft() + c.eye_anchor;
  const qreal e = c.eye_size;
  const qreal g = c.eye_gap;
  const QPointF l_c = eye_origin + QPointF(e / 2.0, e / 2.0);
  const QPointF r_c = eye_origin + QPointF(e + g + e / 2.0, e / 2.0);

  auto draw_eye = [&](const QPointF& cc) {
    if (c.has_eyeball) {
      p.setBrush(Qt::white);
      p.setPen(Qt::NoPen);
      p.drawEllipse(QRectF(cc.x() - e / 2.0, cc.y() - e / 2.0, e, e));
    }
    const qreal pr = e * c.pupil_ratio;
    p.setBrush(QColor("#2d2d2d"));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(cc.x() - pr / 2.0, cc.y() - pr / 2.0, pr, pr));
  };

  draw_eye(l_c);
  draw_eye(r_c);

  if (c.has_mouth) {
    const QPointF mp = body.topLeft() + c.mouth_anchor;
    p.setPen(QPen(QColor("#2d2d2d"), c.mouth_h, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(mp.x(), mp.y()), QPointF(mp.x() + c.mouth_w, mp.y()));
  }

  p.restore();
}