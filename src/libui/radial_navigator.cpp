#include "radial_navigator.h"

#include <QApplication>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QParallelAnimationGroup>
#include <QToolTip>
#include <QVariantAnimation>
#include <QtMath>

RadialNavigator::RadialNavigator(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_Hover, true);
  setMinimumSize(300, 300);
  menuOpen_ = true;  // 默认展开
  progress_ = 1.0;   // 按钮直接到位（初始不播放动画）
  initAnimation();
  // Esc / 外部点击收起（应用级事件过滤）
  qApp->installEventFilter(this);
}

RadialNavigator::~RadialNavigator() {
  qApp->removeEventFilter(this);
}

void RadialNavigator::addButton(const QPixmap& icon,
                                  const QString& title,
                                  std::function<void()> onClicked,
                                  const QString& tooltip,
                                  const QColor& background) {
  Button btn;
  btn.icon = icon;
  btn.title = title;
  btn.onClicked = std::move(onClicked);
  btn.tooltip = tooltip;
  btn.backgroundColor = background;
  buttons_.append(btn);
  update();
}

void RadialNavigator::setCurrentIndex(int index) {
  if (index == currentIndex_) {
    return;
  }
  if (currentIndex_ >= 0 && currentIndex_ < buttons_.size()) {
    buttons_[currentIndex_].selected = false;
  }
  currentIndex_ = index;
  if (index >= 0 && index < buttons_.size()) {
    buttons_[index].selected = true;
  }
  update();
}

// ---- 事件 ----
bool RadialNavigator::event(QEvent* event) {
  switch (event->type()) {
    case QEvent::HoverEnter:
    case QEvent::HoverMove:
      updateHover(static_cast<QHoverEvent*>(event)->pos());
      return true;
    case QEvent::HoverLeave:
      for (Button& btn : buttons_) {
        btn.hover = false;
      }
      switchHover_ = false;
      update();
      return true;
    case QEvent::ToolTip: {
      auto* he = static_cast<QHelpEvent*>(event);
      const bool buttons_visible =
          menuOpen_ || anim_group_->state() == QAbstractAnimation::Running;
      int index = buttons_visible ? buttonAt(he->pos()) : -1;
      QString tip;
      if (index >= 0) {
        tip = buttons_[index].tooltip;
      } else if (switchHover_) {
        tip = menuOpen_ ? QStringLiteral("收起") : QStringLiteral("展开");
      }
      if (!tip.isEmpty()) {
        QToolTip::showText(he->globalPos(), tip, this);
      }
      return true;
    }
    default:
      break;
  }
  return QWidget::event(event);
}

bool RadialNavigator::eventFilter(QObject* obj, QEvent* event) {
  // 导航隐藏（top_bar 开关关闭）时不响应 Esc / 外部点击收起，
  // 避免隐藏期间把菜单收起（重新显示时保持原展开/收起状态）
  if (!isVisible()) {
    return QWidget::eventFilter(obj, event);
  }
  if (event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    // 模态对话框打开时 Esc 交给对话框，不收起导航
    if (key->key() == Qt::Key_Escape && menuOpen_ &&
        QApplication::activeModalWidget() == nullptr) {
      toggleMenu();
      return true;
    }
  } else if (event->type() == QEvent::MouseButtonPress) {
    auto* mouse = static_cast<QMouseEvent*>(event);
    QRect global_rect(mapToGlobal(QPoint(0, 0)), size());
    // 点击导航几何之外且菜单展开 → 收起
    if (menuOpen_ && !global_rect.contains(mouse->globalPos())) {
      toggleMenu();
    }
  }
  return QWidget::eventFilter(obj, event);
}

void RadialNavigator::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mouseReleaseEvent(event);
    return;
  }
  const QPoint pos = event->pos();

  // 中央开关
  const QPointF center(width() / 2.0, height() / 2.0);
  const qreal switch_r = itemSize() / 2.0;
  if (QLineF(QPointF(pos), center).length() <= switch_r) {
    toggleMenu();
    event->accept();
    return;
  }

  // 圆形按钮
  if (menuOpen_ && anim_group_->state() != QAbstractAnimation::Running) {
    int index = buttonAt(pos);
    if (index >= 0) {
      setCurrentIndex(index);
      if (buttons_[index].onClicked) {
        buttons_[index].onClicked();
      }
      event->accept();
      return;
    }
  }
  QWidget::mouseReleaseEvent(event);
}

// ---- 绘制 ----
void RadialNavigator::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                         QPainter::SmoothPixmapTransform);

  // 收起态不绘制圆盘与按钮（仅留中央开关）
  if (menuOpen_ || anim_group_->state() == QAbstractAnimation::Running) {
    drawBackdrop(&painter);
    for (int i = 0; i < buttons_.size(); ++i) {
      drawButton(&painter, i);
    }
  }

  drawSwitch(&painter);
}

void RadialNavigator::drawBackdrop(QPainter* painter) {
  const QPointF center(width() / 2.0, height() / 2.0);
  // 圆盘半径留 6px 边距，避免顶到 widget 边界导致显示不完整
  const qreal radius = qMin(width(), height()) / 2.0 - 6.0;
  drawGlow(painter, center, radius + 4.0, QColor(0, 0, 0, 36));
  painter->setPen(QPen(QColor(255, 255, 255, 100), 1));
  painter->setBrush(QColor(255, 255, 255, 140));
  painter->drawEllipse(center, radius, radius);
}

void RadialNavigator::drawButton(QPainter* painter, int index) {
  const Button& btn = buttons_[index];
  const QPointF c = buttonCenter(index);
  const qreal r = buttonRadius(index);
  if (r < 1.0) {
    return;
  }

  // 阴影 + 悬停/选中柔光
  drawGlow(painter, c + QPointF(0.0, 2.0), r + 2.0, QColor(0, 0, 0, 26));
  if (btn.hover || btn.selected) {
    drawGlow(painter, c, r + 6.0, QColor(0, 120, 212, btn.hover ? 40 : 26));
  }

  // 主体
  QColor bg = QColor(255, 255, 255, 128);
  if (btn.hover) {
    bg = QColor(0, 120, 212, 30);
  } else if (btn.selected) {
    bg = QColor(0, 120, 212, 40);
  } else if (btn.backgroundColor.isValid()) {
    bg = btn.backgroundColor;
    bg.setAlpha(128);
  }
  painter->setBrush(bg);
  painter->setPen(btn.selected ? QPen(QColor(0, 120, 212), 1.5)
                               : QPen(QColor(255, 255, 255, 100), 1));
  painter->drawEllipse(c, r, r);

  // 图标（上，与下方文字保持间距）
  const qreal icon_size = r * 0.76;
  const QRectF icon_rect(c.x() - icon_size / 2.0, c.y() - r * 0.45, icon_size,
                         icon_size);
  painter->drawPixmap(icon_rect, btn.icon, btn.icon.rect());

  // 文字（下，自动缩号）
  if (!btn.title.isEmpty()) {
    QFont font = painter->font();
    font.setPixelSize(r * 0.3);
    font.setBold(btn.selected);
    painter->setFont(font);
    painter->setPen(btn.selected ? QColor(0, 120, 212)
                                 : QColor(0x1a, 0x1a, 0x1a));
    QRectF text_rect = painter->fontMetrics().boundingRect(btn.title);
    const qreal max_w = r * 1.7;
    if (text_rect.width() > 1.0 && text_rect.width() > max_w) {
      font.setPixelSize(qMax(8.0, r * 0.3 * max_w / text_rect.width()));
      painter->setFont(font);
      text_rect = painter->fontMetrics().boundingRect(btn.title);
    }
    text_rect.moveCenter(QPointF(c.x(), c.y() + r * 0.5));
    painter->drawText(text_rect, Qt::AlignCenter, btn.title);
  }
}

void RadialNavigator::drawSwitch(QPainter* painter) {
  const QPointF center(width() / 2.0, height() / 2.0);
  const qreal r = itemSize() / 2.0;
  drawGlow(painter, center, r + 4.0, QColor(0, 0, 0, 30));
  painter->setBrush(QColor(255, 255, 255, 225));
  painter->setPen(switchHover_ ? QPen(QColor(0, 120, 212), 1.5)
                               : QPen(QColor(255, 255, 255, 150), 1));
  painter->drawEllipse(center, r, r);

  // + / × 图标（收起显示 +，展开显示 ×）
  painter->setPen(QPen(QColor(0x1a, 0x1a, 0x1a), 2.0, Qt::SolidLine,
                       Qt::RoundCap, Qt::RoundJoin));
  const qreal len = r * 0.45;
  if (menuOpen_) {
    painter->drawLine(center + QPointF(-len, -len), center + QPointF(len, len));
    painter->drawLine(center + QPointF(-len, len), center + QPointF(len, -len));
  } else {
    painter->drawLine(center + QPointF(-len, 0.0), center + QPointF(len, 0.0));
    painter->drawLine(center + QPointF(0.0, -len), center + QPointF(0.0, len));
  }
}

void RadialNavigator::drawGlow(QPainter* painter,
                                 const QPointF& center,
                                 qreal radius,
                                 const QColor& color) {
  // 多层同心圆模拟羽化柔光（Qt 无原生 blur）
  constexpr int kLayers = 5;
  painter->save();
  painter->setPen(Qt::NoPen);
  for (int i = kLayers; i >= 1; --i) {
    QColor layer = color;
    layer.setAlpha(color.alpha() * (kLayers - i + 1) / kLayers);
    painter->setBrush(layer);
    painter->drawEllipse(center, radius * i / kLayers, radius * i / kLayers);
  }
  painter->restore();
}

// ---- 几何 ----
qreal RadialNavigator::baseRadius() const {
  return qMin(width(), height()) * 0.38;
}

qreal RadialNavigator::itemSize() const {
  return qMin(width(), height()) * 0.18;
}

qreal RadialNavigator::buttonProgress(int index) const {
  const int n = buttons_.size();
  if (n == 0) {
    return 0.0;
  }
  // 收起不延迟（HTML :not(.open) delay 0）；展开按索引逐项延迟
  if (!opening_) {
    return progress_;
  }
  const qreal stagger = kStaggerMs / kAnimationMs;
  const qreal p = (progress_ - index * stagger) / (1.0 - (n - 1) * stagger);
  return qBound(0.0, p, 1.0);
}

QPointF RadialNavigator::buttonCenter(int index) const {
  const int n = buttons_.size();
  const qreal progress = buttonProgress(index);
  const qreal r = progress * baseRadius();
  const qreal angle = qDegreesToRadians(index * 360.0 / n + 90.0 + rotateDeg_);
  return QPointF(width() / 2.0 + r * std::cos(angle),
                 height() / 2.0 - r * std::sin(angle));
}

qreal RadialNavigator::buttonRadius(int index) const {
  qreal r = itemSize() / 2.0;
  if (buttons_[index].hover) {
    r *= 1.08;
  }
  return r;
}

int RadialNavigator::buttonAt(const QPoint& pos) const {
  for (int i = buttons_.size() - 1; i >= 0; --i) {
    const QPointF c = buttonCenter(i);
    const qreal r = buttonRadius(i);
    const QPointF d = QPointF(pos) - c;
    if (d.x() * d.x() + d.y() * d.y() <= r * r) {
      return i;
    }
  }
  return -1;
}

// ---- 动画与交互 ----
void RadialNavigator::initAnimation() {
  anim_group_ = new QParallelAnimationGroup(this);

  anim_progress_ = new QVariantAnimation(this);
  anim_progress_->setDuration(kAnimationMs);
  connect(anim_progress_, &QVariantAnimation::valueChanged,
          [this](const QVariant& value) {
            progress_ = value.toDouble();
            update();
          });

  anim_rotate_ = new QVariantAnimation(this);
  anim_rotate_->setDuration(kAnimationMs);
  connect(anim_rotate_, &QVariantAnimation::valueChanged,
          [this](const QVariant& value) {
            rotateDeg_ = value.toDouble();
            update();
          });

  anim_group_->addAnimation(anim_progress_);
  anim_group_->addAnimation(anim_rotate_);
  connect(anim_group_, &QParallelAnimationGroup::finished, [this]() {
    menuOpen_ = opening_;
    rotateDeg_ = 0.0;  // 旋转末态归 0（360°≡0，避免下次多转一圈）
    update();
  });
}

void RadialNavigator::toggleMenu() {
  if (anim_group_->state() == QAbstractAnimation::Running) {
    return;
  }
  opening_ = !menuOpen_;
  if (opening_) {
    // 展开：按钮从中心扩散 + 旋转一周，末态归 0
    anim_progress_->setStartValue(progress_);
    anim_progress_->setEndValue(1.0);
    anim_rotate_->setStartValue(rotateDeg_);
    anim_rotate_->setEndValue(360.0);
  } else {
    // 收起：按钮收回中心 + 旋转归位
    anim_progress_->setStartValue(progress_);
    anim_progress_->setEndValue(0.0);
    anim_rotate_->setStartValue(rotateDeg_);
    anim_rotate_->setEndValue(0.0);
  }
  anim_group_->start();
}

void RadialNavigator::updateHover(const QPoint& pos) {
  // 收起态按钮收拢到中心，不参与命中，只做开关 hover
  const bool buttons_visible =
      menuOpen_ || anim_group_->state() == QAbstractAnimation::Running;
  const int index = buttons_visible ? buttonAt(pos) : -1;
  for (int i = 0; i < buttons_.size(); ++i) {
    buttons_[i].hover = (i == index);
  }
  const QPointF center(width() / 2.0, height() / 2.0);
  const qreal r = itemSize() / 2.0;
  switchHover_ = (QLineF(QPointF(pos), center).length() <= r);
  update();
}
