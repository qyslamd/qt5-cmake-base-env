#include "tour_bubble_view.h"

#include <QAbstractAnimation>
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

TourBubbleView::TourBubbleView(const TourCountdown* countdown, QWidget* overlay)
    : QWidget(overlay),
      countdown_(countdown),
      overlay_(overlay),
      countdown_label_(new QLabel(this)),
      title_label_(new QLabel(this)),
      text_label_(new QLabel(this)),
      skip_btn_(new QPushButton(QStringLiteral("跳过"), this)),
      prev_btn_(new QPushButton(QStringLiteral("上一步"), this)),
      next_btn_(new QPushButton(QStringLiteral("下一步"), this)),
      anime_(new QPropertyAnimation(this)) {
  // 这个名字是 funq 用例的寻址锚点（tourOverlay::bubble_self::bubble_text），
  // 与"气泡必须是遮罩的直接子控件"一样，不能改
  setObjectName(QStringLiteral("bubble_self"));
  initUi();
  initSignals();

  anime_->setTargetObject(this);
  anime_->setPropertyName("pos");
  anime_->setDuration(kAnimeDur);
}

void TourBubbleView::initUi() {
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

void TourBubbleView::initSignals() {
  // 停表在 TourOverlay 那边做：三个按钮与自动挡翻页走同一个出口
  connect(skip_btn_, &QPushButton::clicked, this, &TourBubbleView::skipClicked);
  connect(prev_btn_, &QPushButton::clicked, this, &TourBubbleView::prevClicked);
  connect(next_btn_, &QPushButton::clicked, this, &TourBubbleView::nextClicked);

  connect(countdown_, &TourCountdown::changed, this,
          &TourBubbleView::updateCountdownDisplay);
}

void TourBubbleView::setTitle(const QString& title) {
  if (title_ != title) {
    title_ = title;
    title_label_->setText(title);
  }
}

void TourBubbleView::setText(const QString& text) {
  if (text_ != text) {
    text_ = text;
    text_label_->setText(text);
  }
}

void TourBubbleView::setNavigation(bool can_prev, bool can_next) {
  prev_btn_->setEnabled(can_prev);
  next_btn_->setText(can_next ? QStringLiteral("下一步")
                              : QStringLiteral("完成！"));
}

void TourBubbleView::setButtonsVisible(bool visible) {
  skip_btn_->setVisible(visible);
  prev_btn_->setVisible(visible);
  next_btn_->setVisible(visible);
}

void TourBubbleView::setBubbleVisible(bool visible) {
  bubble_visible_ = visible;
  syncVisibility();
}

void TourBubbleView::updateCountdownDisplay() {
  // 倒计时的显示**只有这一处决定**：秒数、显隐、进度条画不画都在这儿落地。
  // paintCountdown 只读下面这个布尔，不再自己问一遍倒计时
  countdown_shown_ = countdown_->showing();
  countdown_label_->setText(QString::number(countdown_->remaining()));
  countdown_label_->setVisible(countdown_shown_);
  update();
}

void TourBubbleView::syncVisibility() {
  // 显隐必须单独同步：实操模式下气泡挂在 viewport 上，不随遮罩的显隐走；
  // 而讲解模式下遮罩 hide 时显式藏过它，**被显式隐藏的子控件不会随父 show()
  // 回来**（Qt 的 showChildren 会跳过它），第二遍就只剩遮罩了。
  //
  // raise 也不能省：遮罩每次 execute() 都会被 setParent 回 viewport（插入即
  // 最上层），漏了 raise 半透明遮罩就叠在气泡上
  const bool show = overlay_->isVisible() && bubble_visible_;
  setVisible(show);
  if (show) {
    raise();
  }
}

void TourBubbleView::detachToTopLevel() {
  setParent(nullptr);
  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
  // setParent 会把控件隐藏，需要显式恢复（此时遮罩已 show）
  syncVisibility();
}

void TourBubbleView::attachToOverlay() {
  setParent(overlay_);
  setWindowFlags(Qt::Widget);
  syncVisibility();
}

void TourBubbleView::followAnchor(const QRect& anchor, const QSize& viewport) {
  // 算法在 tour_bubble_placer（纯几何、有单测）；这里换算坐标、交给动画，
  // 并把"有没有靶"落在 has_anchor_ 上
  has_anchor_ = !anchor.isNull();
  const BubblePlacement placement =
      PlaceBubble(anchor, size(), viewport, kLayoutMargin - 15);
  // 挂在遮罩上时是遮罩局部坐标；提成顶层窗口之后是屏幕坐标，得补遮罩原点
  const QPoint origin =
      isWindow() ? overlay_->mapToGlobal(QPoint(0, 0)) : QPoint(0, 0);
  moveTo(origin + placement.top_left, placement.side);
}

void TourBubbleView::moveTo(const QPoint& top_left, BubbleSide side) {
  // 居中那种不带方向（没有目标可依托），**不改箭头**
  const bool arrow_changes =
      side != BubbleSide::kCentred && side != side_;  // NOLINT
  // 位置与朝向都没动、也没有在途动画：没什么可搬的。**不早退的话每一步都会起
  // 一次动画**——updateUi 每步都调到这儿，而多数步骤气泡原位不动
  if (top_left == pos() && !arrow_changes &&
      anime_->state() == QAbstractAnimation::Stopped) {
    return;
  }
  anime_->setStartValue(pos());
  anime_->setEndValue(top_left);
  anime_->start();
  if (arrow_changes) {
    side_ = side;
    // 位置没动的场合也得重画，否则箭头还指着原来那边（位置变了控件自己会重画）
    update();
  }
}

void TourBubbleView::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  paintBubble(&p);
  paintCountdown(&p);
}

void TourBubbleView::paintBubble(QPainter* painter) {
  const int margin = kLayoutMargin - 9;
  const QRectF bubble_rect =
      rect().marginsRemoved(QMargins(margin, margin, margin, margin));

  QPainterPath path;
  path.addRoundedRect(bubble_rect, kCornerRadius, kCornerRadius);
  // 有靶才画箭头；纯讲解步（居中）就是一个干净的圆角矩形
  if (has_anchor_) {
    path.addPath(arrowPath());
  }
  painter->fillPath(path, QBrush(TourBubbleColor()));
}

void TourBubbleView::paintCountdown(QPainter* painter) {
  // 画不画由 updateCountdownDisplay 落地（见那里的注释），这里只读它
  if (!countdown_shown_) {
    return;
  }

  const int margin = kLayoutMargin - 9;
  QRectF bar_rect =
      rect().marginsRemoved(QMargins(margin, margin, margin, margin));

  bar_rect.setWidth(bar_rect.width() * (1 - countdown_->elapsed()));

  QPainterPath path;
  path.addRoundedRect(bar_rect, kCornerRadius, kCornerRadius);
  // 倒计时条用 accent 淡色
  QColor bar = TourAccentColor();
  bar.setAlpha(140);
  painter->fillPath(path, QBrush(bar));
}

QPainterPath TourBubbleView::arrowPath() const {
  // 箭头画在气泡的**反面**：side_ 是气泡在目标的哪一侧，尖端朝目标
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
