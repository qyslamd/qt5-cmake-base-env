#include "tour_bubble_view.h"

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QVBoxLayout>

#include "tour_colors.h"
#include "tour_countdown.h"
#include "tour_overlay_mask.h"

BubbleView::BubbleView(const OverlayMask* mask, const TourCountdown* countdown,
                       QWidget* overlay)
    : QWidget(overlay),
      mask_(mask),
      countdown_(countdown),
      overlay_(overlay),
      countdown_label_(new QLabel(this)),
      title_label_(new QLabel(this)),
      text_label_(new QLabel(this)),
      skip_btn_(new QPushButton(QStringLiteral("跳过"), this)),
      prev_btn_(new QPushButton(QStringLiteral("上一步"), this)),
      next_btn_(new QPushButton(QStringLiteral("下一步"), this)),
      anime_(new QPropertyAnimation(this)) {
  setObjectName(QStringLiteral("bubble_self"));
  initUi();
  initSignals();

  anime_->setTargetObject(this);
  anime_->setPropertyName("pos");
  anime_->setDuration(kAnimeDur);
}

void BubbleView::initUi() {
  // 气泡底由自绘（paintBubble）完成，背景保持透明，让遮罩/主窗口内容透出
  setAutoFillBackground(false);
  setAttribute(Qt::WA_TranslucentBackground);
  setMinimumWidth(300);
  setMinimumHeight(200);

  auto* main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(kLayoutMargin, kLayoutMargin, kLayoutMargin,
                                  kLayoutMargin);
  main_layout->setSpacing(8);

  countdown_label_->setAlignment(Qt::AlignRight);
  countdown_label_->setObjectName(QStringLiteral("bubble_countdown"));
  countdown_label_->hide();

  title_label_->setAlignment(Qt::AlignCenter);
  title_label_->setObjectName(QStringLiteral("bubble_title"));

  text_label_->setObjectName(QStringLiteral("bubble_text"));
  text_label_->setWordWrap(true);

  auto* button_row = new QHBoxLayout();
  button_row->setObjectName(QStringLiteral("horizontalLayout"));
  button_row->addItem(
      new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

  skip_btn_->setObjectName(QStringLiteral("bubble_skip"));
  button_row->addWidget(skip_btn_);

  prev_btn_->setObjectName(QStringLiteral("bubble_prev"));
  button_row->addWidget(prev_btn_);

  next_btn_->setObjectName(QStringLiteral("bubble_next"));
  button_row->addWidget(next_btn_);

  main_layout->addWidget(countdown_label_);
  main_layout->addWidget(title_label_);
  main_layout->addWidget(text_label_);
  main_layout->addLayout(button_row);
}

void BubbleView::initSignals() {
  // 点哪个按钮都先停表（到点自动翻页那一趟也走同一个出口，见
  // TourOverlay::initSignals）
  connect(skip_btn_, &QPushButton::clicked, this, &BubbleView::skipClicked);
  connect(prev_btn_, &QPushButton::clicked, this, &BubbleView::prevClicked);
  connect(next_btn_, &QPushButton::clicked, this, &BubbleView::nextClicked);

  connect(countdown_, &TourCountdown::changed, this,
          &BubbleView::syncCountdown);
}

void BubbleView::setTitle(const QString& title) {
  if (title_ != title) {
    title_ = title;
    title_label_->setText(title);
  }
}

void BubbleView::setText(const QString& text) {
  if (text_ != text) {
    text_ = text;
    text_label_->setText(text);
  }
}

void BubbleView::setNavigation(bool can_prev, bool can_next) {
  prev_btn_->setEnabled(can_prev);
  next_btn_->setText(can_next ? QStringLiteral("下一步")
                              : QStringLiteral("完成！"));
}

void BubbleView::setButtonsVisible(bool visible) {
  skip_btn_->setVisible(visible);
  prev_btn_->setVisible(visible);
  next_btn_->setVisible(visible);
}

void BubbleView::setBubbleVisible(bool visible) {
  bubble_visible_ = visible;
  syncVisibility();
}

void BubbleView::syncCountdown() {
  countdown_label_->setText(QString::number(countdown_->remaining()));
  countdown_label_->setVisible(countdown_->showing());
  update();
}

void BubbleView::syncVisibility() {
  // 气泡的显隐必须单独同步，两种父级都躲不掉：
  //   实操模式——气泡挂在 viewport 上，不随遮罩的父子关系显隐
  //   讲解模式——气泡挂在遮罩上，但遮罩 hide 时显式藏过它，而 Qt 里被显式
  //             隐藏的子控件不会随父控件 show() 回来（showChildren 会跳过
  //             isHidden() 的子控件），于是第二遍只剩遮罩、气泡不见了
  //
  // raise 不能省：execute() 每次都会把遮罩 setParent 回 viewport（插入即
  // 最上层），而 TourOverlay::setInteractive 在状态未变时早退、不会重排层级
  // ——漏了 raise，半透明遮罩就会叠在气泡上（点击穿透所以按钮还能点，但看着
  // 像被阴影盖住，第二次运行起偶发）
  const bool show = overlay_->isVisible() && bubble_visible_;
  setVisible(show);
  if (show) {
    raise();
  }
}

void BubbleView::detachToTopLevel() {
  setParent(nullptr);
  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
  // setParent 会把控件隐藏，需要显式恢复（此时遮罩已 show）
  syncVisibility();
}

void BubbleView::attachToOverlay() {
  setParent(overlay_);
  setWindowFlags(Qt::Widget);
  syncVisibility();
}

void BubbleView::followAnchor(const QSize& viewport) {
  // 算法在 tour_bubble_placer（纯几何、有单测）。这里只剩三件事：换算坐标、
  // 交给动画、记下箭头该朝哪边
  const BubblePlacement placement =
      PlaceBubble(mask_->anchor(), size(), viewport, kLayoutMargin - 15);
  // 气泡挂在遮罩上时是遮罩局部坐标；提成顶层窗口之后是屏幕坐标，得补上遮罩
  // 自己的屏幕原点
  const QPoint origin =
      isWindow() ? overlay_->mapToGlobal(QPoint(0, 0)) : QPoint(0, 0);
  moveTo(origin + placement.top_left, placement.side);
}

void BubbleView::moveTo(const QPoint& top_left, BubbleSide side) {
  anime_->setStartValue(pos());
  anime_->setEndValue(top_left);
  anime_->start();
  // 居中那种不带方向（没有目标可依托），**不改箭头**——与从前一致
  if (side != BubbleSide::kCentred) {
    side_ = side;
  }
}

void BubbleView::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  paintBubble(&p);
  paintCountdown(&p);
}

void BubbleView::paintBubble(QPainter* painter) {
  const int margin = kLayoutMargin - 9;
  const QRectF bubble_rect =
      rect().marginsRemoved(QMargins(margin, margin, margin, margin));

  QPainterPath path;
  path.addRoundedRect(bubble_rect, cornerRadius_, cornerRadius_);
  // 三角箭头只在有高亮矩形依托时绘制；纯讲解步（气泡居中）没有指向
  // 对象，就是一个干净的圆角矩形
  if (!mask_->anchor().isEmpty()) {
    path.addPath(arrowPath());
  }
  painter->fillPath(path, QBrush(TourBubbleColor()));
}

void BubbleView::paintCountdown(QPainter* painter) {
  // 自动挡与"外部驱动的视觉倒计时"都要画这条进度（见 TourCountdown::showing）
  if (!countdown_->showing()) {
    return;
  }

  const int margin = kLayoutMargin - 9;
  QRectF bar_rect =
      rect().marginsRemoved(QMargins(margin, margin, margin, margin));

  bar_rect.setWidth(bar_rect.width() * (1 - countdown_->elapsed()));

  QPainterPath path;
  path.addRoundedRect(bar_rect, cornerRadius_, cornerRadius_);
  // 倒计时条用 accent 淡色（两主题统一降低深浅）
  QColor bar = TourAccentColor();
  bar.setAlpha(140);
  painter->fillPath(path, QBrush(bar));
}

QPainterPath BubbleView::arrowPath() const {
  // 三角箭头：底边贴气泡面向目标一侧的中点（半宽 = margin），尖端顶到
  // 控件边缘。side_ 记录的是气泡在目标的哪一侧，箭头画在相反一侧
  const QRectF area(rect());
  const qreal cx = area.center().x();
  const qreal cy = area.center().y();
  const qreal m = kLayoutMargin - 9;

  QPointF base_a;
  QPointF base_b;
  QPointF tip;
  switch (side_) {
    case BubbleSide::kLeft:  // 气泡在目标的左，箭头朝右
      tip = QPointF(area.right(), cy);
      base_a = QPointF(area.right() - m, cy - m);
      base_b = QPointF(area.right() - m, cy + m);
      break;
    case BubbleSide::kTop:  // 气泡在目标的上，箭头朝下
      tip = QPointF(cx, area.bottom());
      base_a = QPointF(cx - m, area.bottom() - m);
      base_b = QPointF(cx + m, area.bottom() - m);
      break;
    case BubbleSide::kRight:  // 气泡在目标的右，箭头朝左
      tip = QPointF(area.left(), cy);
      base_a = QPointF(area.left() + m, cy - m);
      base_b = QPointF(area.left() + m, cy + m);
      break;
    case BubbleSide::kBottom:  // 气泡在目标的下，箭头朝上
      tip = QPointF(cx, area.top());
      base_a = QPointF(cx - m, area.top() + m);
      base_b = QPointF(cx + m, area.top() + m);
      break;
    default:
      return {};
  }

  QPainterPath arrow;
  arrow.moveTo(base_a);
  arrow.lineTo(base_b);
  arrow.lineTo(tip);
  arrow.closeSubpath();
  return arrow;
}
