#include "login_window.h"

#include <cmath>
#include <functional>

#include <QApplication>
#include <QCheckBox>
#include <QCursor>
#include <QEasingCurve>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

namespace {

const QColor kInk("#1a1a2e");
const QColor kText("#333333");
const QColor kBorder("#e0e0e0");
const QColor kFieldBg("#f5f5f5");

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

// 版式（字号/字重/字距）在代码里表达，颜色/边框归样式表——两条界面路线共用
void ApplyFont(QWidget* w, int pixel_size, int weight = QFont::Normal,
               qreal letter_spacing = 0.0) {
  QFont f = w->font();
  f.setPixelSize(pixel_size);
  f.setWeight(weight);
  if (letter_spacing != 0.0) {
    f.setLetterSpacing(QFont::AbsoluteSpacing, letter_spacing);
  }
  w->setFont(f);
}

// ---- 简单鉴权模拟。真实工程里换成 AuthService::instance().login(...) ----
// 正确账号：admin@example.com / 123456
// 其它组合一律失败（用于看错误抖动动画）
bool MockAuthenticate(const QString& email, const QString& password) {
  return email.compare(QStringLiteral("admin@example.com"),
                       Qt::CaseInsensitive) == 0 &&
         password == QStringLiteral("123456");
}

}  // namespace

// ============================================================
// HoverButton
// ============================================================

HoverButton::HoverButton(const QString& text, Variant v, QWidget* parent)
    : QAbstractButton(parent), text_(text), variant_(v) {
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

void HoverButton::setText(const QString& t) {
  text_ = t;
  QAbstractButton::setText(t);
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
    p.fillPath(clip, kInk);
  } else {
    p.fillPath(clip, kFieldBg);
    p.setPen(QPen(kBorder, 1.5));
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

  const QColor base_text_color =
      (variant_ == Primary) ? QColor(Qt::white) : kText;
  {
    QColor c = base_text_color;
    c.setAlphaF(qMax(0.0, 1.0 - hp_));
    p.setPen(c);

    QRectF tr = r.adjusted(hp_ * 40, 0, 0, 0);
    if (variant_ == Secondary && !icon_.isNull()) {
      QFontMetrics fm(f);
      qreal tw = fm.horizontalAdvance(text_);
      qreal total = 18 + 10 + tw;
      qreal x0 = tr.center().x() - total / 2;
      QRectF icon_rect(x0, tr.center().y() - 9, 18, 18);
      icon_.paint(&p, icon_rect.toRect(), Qt::AlignCenter, QIcon::Normal);
      QRectF text_rect(x0 + 18 + 10, tr.top(), tw + 4, tr.height());
      p.drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft, text_);
    } else {
      p.drawText(tr, Qt::AlignCenter, text_);
    }
  }

  if (hp_ > 0.001) {
    QColor c = Qt::white;
    c.setAlphaF(hp_);
    p.setPen(c);

    QFontMetrics fm(f);
    qreal tw = fm.horizontalAdvance(text_);
    qreal gap = 8;
    qreal arrow_w = 16;
    qreal total = tw + gap + arrow_w;
    qreal x0 = r.center().x() - total / 2;

    p.drawText(QRectF(x0, 0, tw + 2, r.height()),
               Qt::AlignVCenter | Qt::AlignLeft, text_);

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

// ============================================================
// CharacterPanel
// ============================================================

CharacterPanel::CharacterPanel(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setAttribute(Qt::WA_OpaquePaintEvent, false);

  qApp->installEventFilter(this);

  initChars();
  updateScene();

  tick_.setInterval(16);
  connect(&tick_, &QTimer::timeout, this, &CharacterPanel::onTick);
  tick_.start();

  look_timer_.setSingleShot(true);
  look_timer_.setInterval(800);
  connect(&look_timer_, &QTimer::timeout, this, [this]() {
    looking_ = false;
    updateTargets();
  });

  for (int i = 0; i < chars_.size(); ++i) {
    if (chars_[i].has_eyeball)
      scheduleBlink(i);
  }
}

void CharacterPanel::initChars() {
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

void CharacterPanel::updateScene() {
  scene_ = QRectF((width() - 480.0) / 2.0, (height() - 360.0) / 2.0 - 10, 480.0,
                  360.0);
}

void CharacterPanel::resizeEvent(QResizeEvent* e) {
  QWidget::resizeEvent(e);
  updateScene();
  updateTargets();
}

void CharacterPanel::setTyping(bool on) {
  typing_ = on;
  if (on) {
    looking_ = true;
    look_timer_.start();
  } else {
    looking_ = false;
  }
  updateTargets();
}

void CharacterPanel::setPasswordFocused(bool on) {
  pwd_focus_ = on;
  updateTargets();
}

void CharacterPanel::setPasswordVisible(bool on) {
  pwd_visible_ = on;
  if (on) {
    schedulePeek();  // 原版：点开眼睛才开始偷看；重复触发时旧链由世代号作废
  } else {
    peeking_ = false;
    ++peek_gen_;
  }
  updateTargets();
}

void CharacterPanel::setHasPassword(bool on) {
  has_pwd_ = on;
  if (!on) {
    peeking_ = false;  // 密码清空就不看了
    ++peek_gen_;
  }
  updateTargets();
}

void CharacterPanel::triggerError() {
  error_ = true;
  error_shake_ = 0.8;
  const int gen = ++error_gen_;
  updateTargets();
  QTimer::singleShot(2500, this, [this, gen]() {
    if (error_gen_ != gen)
      return;
    error_ = false;
    updateTargets();
  });
}

void CharacterPanel::scheduleBlink(int idx) {
  if (idx >= chars_.size())
    return;
  const int delay = 3000 + QRandomGenerator::global()->bounded(4000);
  QTimer::singleShot(delay, this, [this, idx]() {
    if (idx >= chars_.size())
      return;
    chars_[idx].blinking = true;
    QTimer::singleShot(140, this, [this, idx]() {
      if (idx >= chars_.size())
        return;
      chars_[idx].blinking = false;
      scheduleBlink(idx);
    });
  });
}

void CharacterPanel::schedulePeek() {
  if (!(has_pwd_ && pwd_visible_)) {
    return;  // 条件不再成立就不再排下一次
  }
  const int gen = ++peek_gen_;  // 新链开跑，旧的作废（反复点眼睛不会叠出多条）
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

bool CharacterPanel::eventFilter(QObject*, QEvent* ev) {
  if (ev->type() == QEvent::MouseMove) {
    auto* me = static_cast<QMouseEvent*>(ev);
    mouse_ = mapFromGlobal(me->globalPos());
    if (!error_)
      updateTargets();
  }
  return false;
}

void CharacterPanel::updateTargets() {
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
      // 眼睛定住；只有第一个角色周期性偷看（瞳孔甩向密码框，800ms 后收回）
      eye_move_t = QPointF(-15, -10);
      pupil_t = (i == 0 && peeking_) ? QPointF(4, 5) : QPointF(-4, -4);
    } else if (looking_) {
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
      if (i == 0)
        height_t = 40;
    } else if (looking_) {
      skew_t = (i == 1) ? 10 : -12;
      if (i == 0) {
        offset_t = QPointF(30, 0);
        height_t = 40;
      }
      if (i == 1)
        offset_t = QPointF(20, 0);
    } else {
      const QPointF body_world = scene_.topLeft() + c.base.center();
      qreal dx = mouse_.x() - body_world.x();
      skew_t = qBound(-6.0, -dx / 130.0, 6.0);
      if (i == 1)
        skew_t *= 1.5;
    }
    c.skew_t = skew_t;
    c.height_t = height_t;
    c.offset_t = offset_t;
  }
}

void CharacterPanel::onTick() {
  const qreal k = 0.20;

  if (error_shake_ > 0)
    error_shake_ -= 0.016;

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
  update();
}

void CharacterPanel::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);

  drawBackground(p);

  p.save();
  p.translate(scene_.topLeft());
  for (int i = 0; i < chars_.size(); ++i)
    drawChar(p, i);
  p.restore();

  drawLogo(p);
  drawFooter(p);
}

void CharacterPanel::drawBackground(QPainter& p) {
  QLinearGradient g(0, 0, width(), height());
  g.setColorAt(0.0, QColor("#d4d0dc"));
  g.setColorAt(0.5, QColor("#c8c4d0"));
  g.setColorAt(1.0, QColor("#bbb7c5"));
  p.fillRect(rect(), g);

  {
    const QPointF c(width() * 0.85, height() * 0.25);
    QRadialGradient rg(c, 180);
    rg.setColorAt(0.0, QColor(180, 170, 200, 90));
    rg.setColorAt(1.0, QColor(180, 170, 200, 0));
    p.fillRect(rect(), rg);
  }
  {
    const QPointF c(width() * 0.15, height() * 0.85);
    QRadialGradient rg(c, 220);
    rg.setColorAt(0.0, QColor(200, 195, 210, 70));
    rg.setColorAt(1.0, QColor(200, 195, 210, 0));
    p.fillRect(rect(), rg);
  }
}

void CharacterPanel::drawLogo(QPainter& p) {
  const QRectF icon(48, 40, 28, 28);

  QPainterPath star;
  const QPointF c = icon.center();
  const qreal R = 10, r = 4;
  for (int i = 0; i < 8; ++i) {
    const qreal a = M_PI / 4 * i - M_PI / 2;
    const qreal rr = (i % 2 == 0) ? R : r;
    const QPointF pt(c.x() + std::cos(a) * rr, c.y() + std::sin(a) * rr);
    if (i == 0)
      star.moveTo(pt);
    else
      star.lineTo(pt);
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

  QFont f = font();
  f.setPointSizeF(11.5);
  f.setWeight(QFont::DemiBold);
  p.setFont(f);
  p.setPen(Qt::white);
  p.drawText(QRectF(84, 40, 240, 28), Qt::AlignVCenter | Qt::AlignLeft,
             logo_text_);
}

void CharacterPanel::drawFooter(QPainter& p) {
  QFont f = font();
  f.setPointSizeF(9.5);
  p.setFont(f);
  p.setPen(QColor(80, 70, 90, 180));

  qreal x = 48;
  const qreal y = height() - 40;
  QFontMetrics fm(f);
  for (const QString& s : footer_links_) {
    const qreal w = fm.horizontalAdvance(s);
    p.drawText(QRectF(x, y, w + 4, 18), Qt::AlignVCenter | Qt::AlignLeft, s);
    x += w + 28;
  }
}

void CharacterPanel::drawChar(QPainter& p, int idx) {
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
    if (c.has_eyeball)
      pr2 = pr * qMax(0.15, c.eye_h);
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

// ============================================================
// LoginWindow
// ============================================================

LoginWindow::LoginWindow(QWidget* parent) : QWidget(parent) {
  setWindowTitle(QStringLiteral("登录"));
  setMinimumSize(900, 580);
  resize(1100, 700);
  initUi();
  initSignals();
}

void LoginWindow::initUi() {
  auto* root = new QHBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  left_ = new CharacterPanel(this);
  left_->setObjectName(QStringLiteral("loginLeftPanel"));
  left_->setMinimumWidth(440);
  left_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  root->addWidget(left_, 1);

  right_ = new QWidget(this);
  right_->setObjectName(QStringLiteral("loginRightPanel"));
  right_->setMinimumWidth(440);
  right_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  root->addWidget(right_, 1);

  auto* outer = new QVBoxLayout(right_);
  outer->setContentsMargins(40, 40, 40, 40);
  outer->addStretch();

  auto* form_host = new QWidget(right_);
  form_host->setObjectName(QStringLiteral("loginFormHost"));
  form_host->setMaximumWidth(400);
  auto* form = new QVBoxLayout(form_host);
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(0);

  // Sparkle 图标
  {
    auto* row = new QHBoxLayout();
    row->addStretch();
    auto* spark = new QLabel(form_host);
    spark->setObjectName(QStringLiteral("loginSparkle"));
    spark->setFixedSize(32, 32);
    ApplyFont(spark, 24);
    spark->setText(QStringLiteral("✦"));
    spark->setAlignment(Qt::AlignCenter);
    row->addWidget(spark);
    row->addStretch();
    form->addLayout(row);
    form->addSpacing(24);
  }

  // 标题
  {
    auto* h1 = new QLabel(QStringLiteral("欢迎回来！"), form_host);
    h1->setObjectName(QStringLiteral("loginTitle"));
    h1->setAlignment(Qt::AlignCenter);
    ApplyFont(h1, 26, QFont::Bold, -0.5);
    form->addWidget(h1);
    form->addSpacing(4);

    auto* p = new QLabel(QStringLiteral("请输入您的账号信息"), form_host);
    p->setObjectName(QStringLiteral("loginSubtitle"));
    p->setAlignment(Qt::AlignCenter);
    ApplyFont(p, 13);
    form->addWidget(p);
    form->addSpacing(32);
  }

  auto make_label = [&](const QString& name, const QString& text) {
    auto* l = new QLabel(text, form_host);
    l->setObjectName(name);
    ApplyFont(l, 13, QFont::Medium);
    return l;
  };

  // 邮箱
  email_label_ =
      make_label(QStringLiteral("loginEmailLabel"), QStringLiteral("邮箱"));
  form->addWidget(email_label_);
  form->addSpacing(8);

  email_ = new QLineEdit(form_host);
  email_->setObjectName(QStringLiteral("loginEmail"));
  email_->setPlaceholderText(QStringLiteral("you@example.com"));
  email_->setMinimumHeight(40);
  ApplyFont(email_, 14);
  form->addWidget(email_);
  form->addSpacing(20);

  // 密码
  pwd_label_ =
      make_label(QStringLiteral("loginPasswordLabel"), QStringLiteral("密码"));
  form->addWidget(pwd_label_);
  form->addSpacing(8);

  {
    auto* wrap = new QWidget(form_host);
    wrap->setObjectName(QStringLiteral("loginFieldWrap"));
    auto* hl = new QHBoxLayout(wrap);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    password_ = new QLineEdit(wrap);
    password_->setObjectName(QStringLiteral("loginPassword"));
    password_->setPlaceholderText(QStringLiteral("••••••••"));
    password_->setEchoMode(QLineEdit::Password);
    password_->setMinimumHeight(40);
    ApplyFont(password_, 14);
    hl->addWidget(password_, 1);

    eye_btn_ = new QToolButton(wrap);
    eye_btn_->setObjectName(QStringLiteral("loginTogglePwd"));
    eye_btn_->setCursor(Qt::PointingHandCursor);
    eye_btn_->setText(QStringLiteral("◉"));
    ApplyFont(eye_btn_, 16);
    hl->addWidget(eye_btn_, 0, Qt::AlignVCenter);

    form->addWidget(wrap);
  }
  form->addSpacing(20);

  // 记住密码（去掉互联网味的"忘记密码？"）
  {
    auto* row = new QHBoxLayout();
    remember_ = new QCheckBox(QStringLiteral("记住密码"), form_host);
    remember_->setObjectName(QStringLiteral("loginRemember"));
    remember_->setChecked(true);
    ApplyFont(remember_, 13);
    row->addWidget(remember_);
    row->addStretch();
    form->addLayout(row);
    form->addSpacing(24);
  }

  // 错误提示：红字来自底座的角色规则，横幅的壳在项目层
  err_label_ = new QLabel(form_host);
  err_label_->setObjectName(QStringLiteral("loginError"));
  err_label_->setProperty("role", "error");
  err_label_->setVisible(false);
  err_label_->setWordWrap(true);
  form->addWidget(err_label_);
  form->addSpacing(16);

  // 主按钮
  login_btn_ =
      new HoverButton(QStringLiteral("登录"), HoverButton::Primary, form_host);
  login_btn_->setObjectName(QStringLiteral("loginSubmit"));
  form->addWidget(login_btn_);

  outer->addWidget(form_host, 0, Qt::AlignHCenter);
  outer->addStretch();
}

void LoginWindow::initSignals() {
  // 邮箱打字
  connect(email_, &QLineEdit::textEdited, this,
          [this]() { left_->setTyping(true); });
  connect(email_, &QLineEdit::editingFinished, this,
          [this]() { left_->setTyping(false); });

  // 密码打字
  connect(password_, &QLineEdit::textChanged, this,
          [this](const QString& s) { left_->setHasPassword(!s.isEmpty()); });
  connect(password_, &QLineEdit::textEdited, this,
          [this]() { left_->setTyping(true); });
  connect(password_, &QLineEdit::editingFinished, this,
          [this]() { left_->setTyping(false); });

  // 密码框焦点 -> 角色避开视线
  {
    class FocusWatcher : public QObject {
     public:
      std::function<void(bool)> cb;
      bool eventFilter(QObject*, QEvent* e) override {
        if (e->type() == QEvent::FocusIn && cb)
          cb(true);
        if (e->type() == QEvent::FocusOut && cb)
          cb(false);
        return false;
      }
    };
    auto* w = new FocusWatcher();
    w->setParent(password_);  // 随密码框一起销毁
    w->cb = [this](bool in) { left_->setPasswordFocused(in); };
    password_->installEventFilter(w);
  }

  // 眼睛切换
  connect(eye_btn_, &QToolButton::clicked, this, [this]() {
    show_pwd_ = !show_pwd_;
    password_->setEchoMode(show_pwd_ ? QLineEdit::Normal : QLineEdit::Password);
    left_->setPasswordVisible(show_pwd_);
    eye_btn_->setText(show_pwd_ ? QStringLiteral("◎") : QStringLiteral("◉"));
  });

  connect(login_btn_, &QAbstractButton::clicked, this,
          &LoginWindow::onLoginClicked);
}

void LoginWindow::onLoginClicked() {
  const QString em = email_->text().trimmed();
  const QString pw = password_->text();

  err_label_->setVisible(false);

  static const QRegularExpression re(
      QStringLiteral(R"(^[^\s@]+@[^\s@]+\.[^\s@]+$)"));
  if (em.isEmpty() || !re.match(em).hasMatch()) {
    err_label_->setText(QStringLiteral("请输入有效的邮箱地址。"));
    err_label_->setVisible(true);
    left_->triggerError();
    return;
  }
  if (pw.length() < 6) {
    err_label_->setText(QStringLiteral("密码至少需要 6 个字符。"));
    err_label_->setVisible(true);
    left_->triggerError();
    return;
  }

  // 模拟请求延迟，方便看"登录中…"状态
  login_btn_->setText(QStringLiteral("登录中…"));
  login_btn_->setEnabled(false);
  QTimer::singleShot(600, this, [this, em, pw]() {
    login_btn_->setText(QStringLiteral("登录"));
    login_btn_->setEnabled(true);

    // ---- 简单鉴权模拟（真实工程换成 AuthService::instance().login(em,
    // pw)）----
    if (MockAuthenticate(em, pw)) {
      emit loginSucceeded();
      close();
      return;
    }

    err_label_->setText(QStringLiteral("邮箱或密码错误，请重试。"));
    err_label_->setVisible(true);
    left_->triggerError();
  });
}