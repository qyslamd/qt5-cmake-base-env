#include "default_character_animator.h"

#include <cmath>

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QtMath>

namespace {

const QColor kInk("#1a1a2e");
const QColor kFieldBg("#f5f5f5");
const QColor kBorder("#e0e0e0");

qreal lerp(qreal a, qreal b, qreal t) { return a + (b - a) * t; }
QPointF lerp(const QPointF& a, const QPointF& b, qreal t) {
  return a + (b - a) * t;
}

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

DefaultCharacterAnimator::DefaultCharacterAnimator(QObject* parent)
    : CharacterAnimator(parent) {
  initChars();

  // 插值由面板的逐帧定时器驱动（间隔 animationIntervalMs() 报 16ms）；
  // 面板隐藏或策略被替换时定时器停，这里不需要自己的帧循环
  look_timer_.setSingleShot(true);
  look_timer_.setInterval(800);
  connect(&look_timer_, &QTimer::timeout, this, [this]() {
    looking_ = false;
    updateTargets();
  });

  for (int i = 0; i < chars_.size(); ++i) {
    if (chars_[i].has_eyeball) {
      scheduleBlink(i);
    }
  }
}

void DefaultCharacterAnimator::initChars() {
  chars_.clear();

  // 紫色：170x370 @ left=60，底对齐 360
  {
    Char c;
    c.body = QColor("#6c3ff5");
    c.base = QRectF(60, 360 - 370, 170, 370);
    c.corner = 10;
    c.eye_anchor = QPointF(60, 50);
    c.eye_size = 18;
    c.eye_gap = 28;
    c.pupil_ratio = 0.39;
    c.skew = c.skew_t = -5.2;
    chars_.append(c);
  }
  // 黑色：115x290 @ left=220
  {
    Char c;
    c.body = QColor("#2d2d2d");
    c.base = QRectF(220, 360 - 290, 115, 290);
    c.corner = 8;
    c.eye_anchor = QPointF(41, 42);
    c.eye_size = 16;
    c.eye_gap = 20;
    c.pupil_ratio = 0.375;
    c.skew = c.skew_t = -4.1;
    chars_.append(c);
  }
  // 橙色：230x190 @ left=0，上半圆
  {
    Char c;
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
    c.skew = c.skew_t = -5.5;
    chars_.append(c);
  }
  // 黄色：135x215 @ left=290，上半圆
  {
    Char c;
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
    c.skew = c.skew_t = -3.5;
    chars_.append(c);
  }
}

void DefaultCharacterAnimator::updateScene(const QSize& panel_size) {
  const qreal w = panel_size.width();
  const qreal h = panel_size.height();
  scene_ = QRectF((w - 480.0) / 2.0, (h - 360.0) / 2.0 - 10, 480.0, 360.0);
}

void DefaultCharacterAnimator::setInput(const Input& in) {
  const bool size_changed = (in.panel_size != input_.panel_size);

  input_ = in;
  mouse_ = in.mouse_local;
  typing_ = in.typing;
  pwd_focus_ = in.password_focused;
  pwd_visible_ = in.password_visible;
  has_pwd_ = in.has_password;

  if (size_changed) {
    updateScene(in.panel_size);
  }

  // 原逻辑：点开眼睛才开始偷看；重复触发时旧链由世代号作废
  if (pwd_visible_) {
    schedulePeek();
  } else if (!pwd_visible_) {
    // 从可见切到不可见：取消偷看
    if (peeking_) {
      peeking_ = false;
      ++peek_gen_;
    }
  }

  // 密码清空就不看了
  if (!has_pwd_ && peeking_) {
    peeking_ = false;
    ++peek_gen_;
  }

  // 错误态下不跟随鼠标
  if (!error_) {
    updateTargets();
  }
}

void DefaultCharacterAnimator::onValidationFailed() {
  error_ = true;
  error_shake_ = 0.8;
  const int gen = ++error_gen_;
  updateTargets();
  QTimer::singleShot(2500, this, [this, gen]() {
    if (error_gen_ != gen) {
      return;
    }
    error_ = false;
    updateTargets();
  });
}

void DefaultCharacterAnimator::reset() {
  // 世代号作废，旧的 singleShot 回调自然 return
  ++error_gen_;
  ++peek_gen_;
  error_ = false;
  peeking_ = false;
  error_shake_ = 0;
  looking_ = false;
  look_timer_.stop();
}

void DefaultCharacterAnimator::scheduleBlink(int idx) {
  if (idx >= chars_.size()) {
    return;
  }
  const int delay = 3000 + QRandomGenerator::global()->bounded(4000);
  QTimer::singleShot(delay, this, [this, idx]() {
    if (idx >= chars_.size()) {
      return;
    }
    chars_[idx].blinking = true;
    QTimer::singleShot(140, this, [this, idx]() {
      if (idx >= chars_.size()) {
        return;
      }
      chars_[idx].blinking = false;
      scheduleBlink(idx);
    });
  });
}

void DefaultCharacterAnimator::schedulePeek() {
  if (!(has_pwd_ && pwd_visible_)) {
    return;  // 条件不再成立就不再排下一次
  }
  const int gen = ++peek_gen_;  // 新链开跑，旧的作废
  const int delay = 2000 + QRandomGenerator::global()->bounded(3000);
  QTimer::singleShot(delay, this, [this, gen]() {
    if (gen != peek_gen_ || !(has_pwd_ && pwd_visible_)) {
      return;
    }
    peeking_ = true;
    updateTargets();
    QTimer::singleShot(800, this, [this, gen]() {
      if (gen != peek_gen_) {
        return;
      }
      peeking_ = false;
      updateTargets();
      schedulePeek();
    });
  });
}

void DefaultCharacterAnimator::updateTargets() {
  const bool showing_pwd = has_pwd_ && pwd_visible_;
  const bool looking_away = pwd_focus_ && !pwd_visible_;

  for (int i = 0; i < chars_.size(); ++i) {
    Char& c = chars_[i];

    QPointF eye_move_t(0, 0);
    QPointF pupil_t(0, 0);
    c.sad_t = false;

    if (error_) {
      eye_move_t = QPointF(-8, 6);
      pupil_t = QPointF(-3, 4);
      c.sad_t = true;
    } else if (looking_away) {
      eye_move_t = QPointF(-18, -12);
      pupil_t = QPointF(-5, -5);
    } else if (showing_pwd) {
      // 眼睛定住；只有第一个角色周期性偷看
      eye_move_t = QPointF(-15, -10);
      pupil_t = (i == 0 && peeking_) ? QPointF(4, 5) : QPointF(-4, -4);
    } else if (typing_) {
      eye_move_t = QPointF(8, 12);
      pupil_t = QPointF(3, 4);
    } else {
      const QPointF eyes_world = scene_.topLeft() + c.base.topLeft() +
                                 c.eye_anchor +
                                 QPointF(c.eye_size / 2.0, c.eye_size / 2.0);
      const QPointF d = mouse_ - eyes_world;
      qreal ex = qBound(-12.0, d.x() / 22.0, 12.0);
      qreal ey = qBound(-8.0, d.y() / 32.0, 8.0);
      eye_move_t = QPointF(ex, ey);
      qreal px = qBound(-4.0, d.x() / 30.0, 4.0);
      qreal py = qBound(-3.0, d.y() / 40.0, 3.0);
      pupil_t = QPointF(px, py);
    }
    c.eye_move_t = eye_move_t;
    c.pupil_t = pupil_t;

    qreal skew_t = 0;
    qreal height_t = 0;
    QPointF offset_t(0, 0);

    if (showing_pwd) {
      skew_t = 0;
    } else if (looking_away) {
      skew_t = (i == 1) ? 12 : -14;
      offset_t = QPointF(-10, 0);
      if (i == 0) {
        height_t = 40;
      }
    } else if (typing_) {
      skew_t = (i == 1) ? 10 : -12;
      if (i == 0) {
        offset_t = QPointF(30, 0);
        height_t = 40;
      }
      if (i == 1) {
        offset_t = QPointF(20, 0);
      }
    } else {
      const QPointF body_world = scene_.topLeft() + c.base.center();
      qreal dx = mouse_.x() - body_world.x();
      skew_t = qBound(-6.0, -dx / 130.0, 6.0);
      if (i == 1) {
        skew_t *= 1.5;
      }
    }
    c.skew_t = skew_t;
    c.height_t = height_t;
    c.offset_t = offset_t;
  }
}

void DefaultCharacterAnimator::tick() {
  const qreal k = 0.20;

  if (error_shake_ > 0) {
    error_shake_ -= 0.016;
  }

  for (Char& c : chars_) {
    c.skew = lerp(c.skew, c.skew_t, k);
    c.height = lerp(c.height, c.height_t, k);
    c.offset = lerp(c.offset, c.offset_t, k);
    c.pupil = lerp(c.pupil, c.pupil_t, k);
    c.eye_move = lerp(c.eye_move, c.eye_move_t, k);
    c.sad = c.sad_t;

    const qreal target_h = c.blinking ? 0.08 : 1.0;
    c.eye_h = lerp(c.eye_h, target_h, 0.55);
  }

  // 本策略没有控件指针，无法调 update()。改为给面板发一个"该重绘了"
  // 的信号，面板收到后调自己的 update()
  emit repaintRequested();
}

void DefaultCharacterAnimator::paint(QPainter& p, const QRectF& rect) const {
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);

  drawBackground(p, rect);

  p.save();
  p.translate(scene_.topLeft());
  for (int i = 0; i < chars_.size(); ++i) {
    drawChar(p, i);
  }
  p.restore();

  drawLogo(p, rect);
  // drawFooter(p, rect);
}

void DefaultCharacterAnimator::drawBackground(QPainter& p,
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

void DefaultCharacterAnimator::drawLogo(QPainter& p, const QRectF& rect) const {
  Q_UNUSED(rect);
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

void DefaultCharacterAnimator::drawFooter(QPainter& p,
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

void DefaultCharacterAnimator::drawChar(QPainter& p, int idx) const {
  const Char& c = chars_[idx];

  QRectF body = c.base;
  body.setTop(body.top() - c.height);

  QPointF shake(0, 0);
  if (error_shake_ > 0) {
    const qreal elapsed = 0.8 - error_shake_;
    const qreal phase = elapsed * 30.0;
    const qreal decay = error_shake_ / 0.8;
    shake.setX(std::sin(phase) * decay * 10.0);
  }

  p.save();

  const QPointF anchor(body.center().x(), body.bottom());
  QTransform tr;
  tr.translate(anchor.x() + c.offset.x() + shake.x(), anchor.y());
  tr.shear(qTan(qDegreesToRadians(c.skew)), 0);
  tr.translate(-anchor.x(), -anchor.y());
  p.setTransform(tr, true);

  p.setPen(Qt::NoPen);
  p.setBrush(c.body);
  if (c.half_circle) {
    p.drawPath(halfCirclePath(body));
  } else {
    p.drawPath(topRoundedPath(body, c.corner, c.corner));
  }

  const QPointF eye_origin = body.topLeft() + c.eye_anchor + c.eye_move;

  const qreal e = c.eye_size;
  const qreal g = c.eye_gap;
  const QPointF l_c = eye_origin + QPointF(e / 2.0, e / 2.0);
  const QPointF r_c = eye_origin + QPointF(e + g + e / 2.0, e / 2.0);

  const qreal eye_h = e * c.eye_h;

  auto draw_eye = [&](const QPointF& cc) {
    if (c.has_eyeball) {
      p.setBrush(Qt::white);
      p.setPen(Qt::NoPen);
      p.drawEllipse(QRectF(cc.x() - e / 2.0, cc.y() - eye_h / 2.0, e, eye_h));
    }
    const qreal pr = e * c.pupil_ratio;
    const QPointF pc = cc + c.pupil;
    p.setBrush(QColor("#2d2d2d"));
    p.setPen(Qt::NoPen);
    qreal pr2 = pr;
    if (c.has_eyeball) {
      pr2 = pr * qMax(0.15, c.eye_h);
    }
    p.drawEllipse(QRectF(pc.x() - pr / 2.0, pc.y() - pr2 / 2.0, pr, pr2));
  };

  draw_eye(l_c);
  draw_eye(r_c);

  if (c.has_mouth) {
    const QPointF mp = body.topLeft() + c.mouth_anchor;
    if (c.sad) {
      QPainterPath mp_path;
      const QRectF mr(mp.x(), mp.y() - c.mouth_h / 2.0, c.mouth_w,
                      c.mouth_h * 2);
      mp_path.moveTo(mr.left(), mr.top());
      mp_path.arcTo(mr, 180, 180);
      p.setBrush(Qt::NoBrush);
      p.setPen(QPen(QColor("#2d2d2d"), 3, Qt::SolidLine, Qt::RoundCap));
      p.drawPath(mp_path);
    } else {
      p.setPen(QPen(QColor("#2d2d2d"), c.mouth_h, Qt::SolidLine, Qt::RoundCap));
      p.drawLine(QPointF(mp.x(), mp.y()), QPointF(mp.x() + c.mouth_w, mp.y()));
    }
  }

  p.restore();
}