#include "tour_homepage.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QFont>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLayout>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPushButton>
#include <QShowEvent>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include "tour_catalog.h"
#include "tour_controller.h"
#include "tour_style.h"

// ---------------------------------------------------------------------------
// TourCard
// ---------------------------------------------------------------------------

TourCard::TourCard(TourTopic* topic, QWidget* parent)
    : QWidget(parent),
      topic_(topic),
      placeholder_(topic == nullptr || topic->stepCount() == 0) {
  // 占位态也交给 QSS（[placeholder="true"]），与底色/边框同一处。
  // 构造期设一次即可，之后不变，所以不需要 unpolish/polish
  setProperty("placeholder", placeholder_);
  initUi();
}

void TourCard::initUi() {
  setObjectName(QStringLiteral("tourCard"));
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_StyledBackground, true);
  setCursor(Qt::PointingHandCursor);
  setMinimumSize(150, 170);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(8);

  icon_label_ = new QLabel(this);
  icon_label_->setObjectName(QStringLiteral("tourCardIcon"));
  icon_label_->setAlignment(Qt::AlignCenter);
  if (topic_ != nullptr && !topic_->icon().isNull()) {
    icon_label_->setPixmap(topic_->icon().scaled(
        QSize(48, 48), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  layout->addWidget(icon_label_);

  title_label_ =
      new QLabel(topic_ != nullptr ? topic_->name() : QString(), this);
  title_label_->setObjectName(QStringLiteral("tourCardTitle"));
  title_label_->setAlignment(Qt::AlignCenter);
  title_label_->setWordWrap(true);
  layout->addWidget(title_label_);

  desc_label_ = new QLabel(
      placeholder_ ? QStringLiteral("敬请期待")
                   : (topic_ != nullptr ? topic_->description() : QString()),
      this);
  desc_label_->setObjectName(QStringLiteral("tourCardDesc"));
  desc_label_->setAlignment(Qt::AlignCenter);
  desc_label_->setWordWrap(true);
  layout->addWidget(desc_label_);
}

void TourCard::setSelected(bool selected) {
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  // 选中态由 QSS 的 [selected="true"] 表达。运行中改属性之后必须重新 polish，
  // 否则样式不重算（见 CLAUDE.md 的控件定位优先级：类型/伪状态 → 动态属性）
  setProperty("selected", selected_);
  style()->unpolish(this);
  style()->polish(this);
  update();
}

void TourCard::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    pressed_ = true;
    update();
    return;
  }
  QWidget::mousePressEvent(event);
}

void TourCard::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton && pressed_) {
    pressed_ = false;
    update();
    if (!placeholder_ && rect().contains(event->pos())) {
      emit clicked();
    }
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

// ---------------------------------------------------------------------------
// TourHomePage
// ---------------------------------------------------------------------------

TourHomePage::TourHomePage(TourController* controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
  // 覆盖层统一挂到顶层窗口：避免被侧边栏等中间父对象的边界裁剪，
  // 同时保证遮罩能盖住整个宿主窗口
  if (parent && parent->window() && parent->window() != parent) {
    setParent(parent->window());
  }

  // 半透明遮罩自绘；不铺不透明底，让主窗口内容透出来。
  setAttribute(Qt::WA_TranslucentBackground);
  setAutoFillBackground(false);
  setFocusPolicy(Qt::StrongFocus);

  initUi();
  initStyleSheet();
  initSignals();
}

int TourHomePage::exec() {
  // 普通 QWidget 没有 QDialog 的事件循环，这里用局部 QEventLoop 模拟。
  // closed() 在 hideEvent 中发出，覆盖"外部直接调 hide()"的情况。
  show();
  raise();

  QEventLoop loop;
  connect(this, &TourHomePage::closed, &loop, &QEventLoop::quit);
  loop.exec();
  return 0;
}

void TourHomePage::setHeading(const QString& title, const QString& subtitle) {
  if (title_label_ != nullptr) {
    title_label_->setText(title);
  }
  if (sub_label_ != nullptr) {
    sub_label_->setText(subtitle);
  }
}

void TourHomePage::initUi() {
  // 居中面板：固定尺寸，QSS #tourHomePanel 可覆盖背景（半透明 + 圆角）；
  // 无 QSS 时用 palette Window 色兜底。阴影由 QGraphicsDropShadowEffect 加。
  panel_ = new QFrame(this);
  panel_->setObjectName(QStringLiteral("tourHomePanel"));
  panel_->setFixedSize(880, 520);
  panel_->setAttribute(Qt::WA_StyledBackground);
  QPalette pal = panel_->palette();
  pal.setColor(QPalette::Window, QColor("#F0F0F0"));
  panel_->setPalette(pal);

  // 面板阴影：挂一个 QGraphicsDropShadowEffect（比在 QSS 里做更省事）
  auto* shadow = new QGraphicsDropShadowEffect(panel_);
  shadow->setColor(QColor(105, 105, 105, 200));
  shadow->setBlurRadius(9);
  shadow->setOffset(0, 0);
  panel_->setGraphicsEffect(shadow);

  auto* panel_layout = new QVBoxLayout(panel_);
  panel_layout->setContentsMargins(28, 20, 28, 20);
  panel_layout->setSpacing(16);

  // 标题行：居中主标题 + 副标题，右上角关闭 ×
  auto* header_layout = new QHBoxLayout;
  header_layout->setContentsMargins(0, 0, 0, 0);
  header_layout->addStretch(1);

  auto* title_box = new QVBoxLayout;
  title_box->setSpacing(4);
  title_box->setAlignment(Qt::AlignCenter);

  // 标题默认取宿主的 applicationName()：引导是通用库，不该写死某个产品的
  // 名字。宿主想显示别的（产品名与程序名不一致）用 setHeading() 改
  const QString app_name = QCoreApplication::applicationName();
  title_label_ = new QLabel(app_name.isEmpty()
                                ? QStringLiteral("欢迎使用")
                                : QStringLiteral("欢迎使用 %1").arg(app_name),
                            panel_);
  title_label_->setObjectName(QStringLiteral("tourHomeTitle"));
  title_label_->setAlignment(Qt::AlignCenter);
  QFont title_font = title_label_->font();
  title_font.setPointSize(20);
  title_font.setBold(true);
  title_label_->setFont(title_font);
  title_box->addWidget(title_label_);

  sub_label_ = new QLabel(QStringLiteral("选择一项开始了解"), panel_);
  sub_label_->setObjectName(QStringLiteral("tourHomeSub"));
  sub_label_->setAlignment(Qt::AlignCenter);
  title_box->addWidget(sub_label_);

  header_layout->addLayout(title_box);
  header_layout->addStretch(1);

  close_btn_ = new QToolButton(panel_);
  close_btn_->setObjectName(QStringLiteral("tourHomeClose"));
  close_btn_->setIcon(style()->standardIcon(QStyle::SP_DockWidgetCloseButton));
  close_btn_->setIconSize(QSize(16, 16));
  close_btn_->setCursor(Qt::PointingHandCursor);
  header_layout->addWidget(close_btn_);

  panel_layout->addLayout(header_layout);

  // 卡片行：居中，rebuildCards() 按 controller_->topics() 往里填卡。
  cards_layout_ = new QHBoxLayout;
  cards_layout_->setSpacing(16);
  cards_layout_->setAlignment(Qt::AlignCenter);
  panel_layout->addLayout(cards_layout_);

  // 底部操作行：自动演示勾选 + 全部演示按钮。
  auto* bottom_layout = new QHBoxLayout;
  bottom_layout->setContentsMargins(0, 0, 0, 0);
  bottom_layout->setSpacing(16);

  auto_check_ = new QCheckBox(QStringLiteral("自动演示"), panel_);
  auto_check_->setObjectName(QStringLiteral("tourAutoCheck"));
  auto_check_->setLayoutDirection(Qt::RightToLeft);
  bottom_layout->addWidget(auto_check_);

  bottom_layout->addStretch(1);

  all_btn_ = new QPushButton(QStringLiteral("全部演示"), panel_);
  all_btn_->setObjectName(QStringLiteral("tourAllBtn"));
  bottom_layout->addWidget(all_btn_);

  panel_layout->addLayout(bottom_layout);
}

void TourHomePage::initStyleSheet() {
  // 样式由 TourStyle 在应用级统一并入（见 tour_style.h）：气泡在实操
  // 模式下会被 reparent 到 viewport，离开首页的子树，只设在首页上盖不到它。
  // 这里不再重复设一份，避免同一份 qss 读两遍、也避免两处优先级打架
  TourStyle::ensureApplied();
}

void TourHomePage::initSignals() {
  connect(close_btn_, &QToolButton::clicked, this, &TourHomePage::hideHomePage);
  connect(all_btn_, &QPushButton::clicked, this, [this] {
    hideHomePage();
    if (controller_ != nullptr) {
      controller_->startAll(auto_check_->isChecked());
    }
  });
}

void TourHomePage::rebuildCards() {
  while (QLayoutItem* item = cards_layout_->takeAt(0)) {
    if (QWidget* w = item->widget()) {
      w->deleteLater();
    }
    delete item;
  }

  if (controller_ == nullptr) {
    return;
  }
  for (TourTopic* topic : controller_->topics()) {
    auto* card = new TourCard(topic, this);
    connect(card, &TourCard::clicked, this,
            [this, topic] { startTopic(topic); });
    cards_layout_->addWidget(card);
  }
}

void TourHomePage::startTopic(TourTopic* topic) {
  if (topic == nullptr || topic->stepCount() == 0) {
    return;
  }
  hideHomePage();
  if (controller_ != nullptr) {
    controller_->startOne(topic, auto_check_->isChecked());
  }
}

void TourHomePage::hideHomePage() {
  // hideEvent 会发 closed()，exec() 的局部事件循环随之退出。
  hide();
}

void TourHomePage::showEvent(QShowEvent* event) {
  // 每次显示前重建卡片（topics 可能已变）。
  rebuildCards();

  // 铺满父窗口客户区并置顶。父窗口 resize 时下次 show 会重新计算；
  // 若需实时跟随，可让父窗口在 resizeEvent 里调用
  // setGeometry(parentWidget()->rect())。
  if (QWidget* p = parentWidget()) {
    setGeometry(p->rect());
  }
  raise();

  // 面板居中
  if (panel_ != nullptr) {
    panel_->move((width() - panel_->width()) / 2,
                 (height() - panel_->height()) / 2);
    panel_->raise();
  }

  QWidget::showEvent(event);
}

void TourHomePage::hideEvent(QHideEvent* event) {
  // 无论是 hideHomePage() 还是外部直接 hide()，都统一从这里发 closed()，
  // 让 exec() 的局部事件循环退出（等价于原 QDialog 关闭时的行为）。
  emit closed();
  QWidget::hideEvent(event);
}

void TourHomePage::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  // 自绘遮罩：半透明黑
  // 不调 QWidget::paintEvent，让子控件各自绘制、不被底色覆盖。
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), QColor(0, 0, 0, 128));
}