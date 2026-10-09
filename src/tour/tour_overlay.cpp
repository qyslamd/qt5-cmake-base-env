#include "tour_overlay.h"

#include <QApplication>
#include <QDebug>
#include <QEasingCurve>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "tour_catalog.h"

namespace {

// 额外靶 → 遮罩局部矩形（《一步多靶设计》§3.2）。**只认控件与矩形**：QPixmap
// 那一种是"洞里贴一张合成图"，多靶要变成 (rect, pixmap) 成对的结构才说得通，
// 不值当（设计 D4）。锚点那一支不走这里——它是唯一处理图片的地方，原样不动
QRect RectForExtraTarget(const QWidget* overlay,
                         const TourStep::Target& target) {
  return std::visit(
      [overlay](auto&& t) -> QRect {
        using T = std::decay_t<decltype(t)>;
        if constexpr (std::is_same_v<T, QWidget*>) {
          if (t == nullptr) {
            return QRect();
          }
          return QRect(overlay->mapFromGlobal(t->mapToGlobal(QPoint(0, 0))),
                       t->size());
        } else if constexpr (std::is_same_v<T, QRect>) {
          if (t.isNull()) {
            return QRect();
          }
          // 假定传进来的是全局坐标系（与 updateUi 里那一支同一条口径）
          return QRect(overlay->mapFromGlobal(t.topLeft()), t.size());
        } else {
          qWarning() << "引导演示：额外靶不支持图片，已忽略";
          return QRect();
        }
      },
      target);
}

}  // namespace

TourOverlay::TourOverlay(QWidget* parent)
    : QWidget(parent),
      bubble_widget_(new QWidget(this)),
      title_label_(new QLabel(bubble_widget_)),
      countdown_label_(new QLabel(bubble_widget_)),
      text_label_(new QLabel(bubble_widget_)),
      skip_btn_(new QPushButton(QStringLiteral("跳过"), bubble_widget_)),
      prev_btn_(new QPushButton(QStringLiteral("上一步"), bubble_widget_)),
      next_btn_(new QPushButton(QStringLiteral("下一步"), bubble_widget_)),
      count_down_timer_(new QTimer(this)) {
  // 独立顶层窗口（不设 Qt 父子，宿主由 SetHost 记）：无边框、不进任务栏，
  // 半透明让宿主内容透出（遮罩底色自绘）
  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
  // 顶层窗口也要有名字：外部工具对没名字的顶层只能编"类名-序号"，
  // 连"遮罩显没显示"都没法查
  setObjectName(QStringLiteral("tourOverlay"));
  setAttribute(Qt::WA_TranslucentBackground);
  setAutoFillBackground(false);
  initUi();
  initOthers();
  initSignals();
}

TourOverlay::~TourOverlay() {
  // 气泡可能已经被 setParent(nullptr) 摘出了父子链（实操模式），不再有人替
  // 我们删它；无条件删一次，两种情况都对
  delete bubble_widget_;
  bubble_widget_ = nullptr;
}

void TourOverlay::setTitle(const QString& title) {
  if (title_ != title) {
    title_ = title;
    title_label_->setText(title);
  }
}

void TourOverlay::setText(const QString& text) {
  if (text_ != text) {
    text_ = text;
    text_label_->setText(text);
  }
}

bool TourOverlay::eventFilter(QObject* obj, QEvent* event) {
  if (obj == bubble_widget_ && event->type() == QEvent::Paint) {
    QPainter p(bubble_widget_);
    p.setRenderHint(QPainter::Antialiasing);
    drawBubble(&p);
    drawCountDownOnBubble(&p);
    return true;
  }
  return QWidget::eventFilter(obj, event);
}

void TourOverlay::initUi() {
  bubble_widget_->setObjectName(QStringLiteral("bubble_self"));
  // 气泡底由自绘（drawBubble）完成，背景保持透明，让遮罩/主窗口内容透出。
  bubble_widget_->setAutoFillBackground(false);
  bubble_widget_->setAttribute(Qt::WA_TranslucentBackground);
  bubble_widget_->setMinimumWidth(300);
  bubble_widget_->setMinimumHeight(200);

  QVBoxLayout* mainLayout = new QVBoxLayout(bubble_widget_);
  mainLayout->setContentsMargins(kLayoutMargin, kLayoutMargin, kLayoutMargin,
                                 kLayoutMargin);
  mainLayout->setSpacing(8);

  countdown_label_->setAlignment(Qt::AlignRight);
  countdown_label_->setObjectName(QStringLiteral("bubble_countdown"));
  countdown_label_->hide();

  title_label_->setAlignment(Qt::AlignCenter);
  title_label_->setObjectName(QStringLiteral("bubble_title"));

  text_label_->setObjectName(QStringLiteral("bubble_text"));
  text_label_->setWordWrap(true);

  QHBoxLayout* horizontalLayout = new QHBoxLayout();
  horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
  horizontalLayout->addItem(
      new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

  skip_btn_->setObjectName(QStringLiteral("bubble_skip"));
  horizontalLayout->addWidget(skip_btn_);

  prev_btn_->setObjectName(QStringLiteral("bubble_prev"));
  horizontalLayout->addWidget(prev_btn_);

  next_btn_->setObjectName(QStringLiteral("bubble_next"));
  horizontalLayout->addWidget(next_btn_);

  mainLayout->addWidget(countdown_label_);
  mainLayout->addWidget(title_label_);
  mainLayout->addWidget(text_label_);
  mainLayout->addLayout(horizontalLayout);
}

void TourOverlay::initOthers() {
  bubble_widget_->installEventFilter(this);

  anime_highlight_ = new QVariantAnimation(this);
  anime_highlight_->setDuration(kAnimeDur);
  anime_highlight_->setEasingCurve(QEasingCurve::InOutCubic);
  connect(anime_highlight_, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& var) {
            anime_highlight_rect_ = var.value<QRect>();
            update();
          });
  connect(anime_highlight_, &QVariantAnimation::finished, this,
          [this] { update(); });

  anime_bubble_ = new QPropertyAnimation(this);
  anime_bubble_->setTargetObject(bubble_widget_);
  anime_bubble_->setPropertyName("pos");
  anime_bubble_->setDuration(kAnimeDur);
}

void TourOverlay::initSignals() {
  connect(skip_btn_, &QPushButton::clicked, this, [this] {
    count_down_timer_->stop();
    emit skipClicked();
  });
  connect(prev_btn_, &QPushButton::clicked, this, [this] {
    count_down_timer_->stop();
    emit prevClicked();
  });
  connect(next_btn_, &QPushButton::clicked, this, [this] {
    count_down_timer_->stop();
    emit nextClicked();
  });

  connect(count_down_timer_, &QTimer::timeout, this, [this] {
    // 走满本次该走的 tick 数（默认 5 秒，ShowCountdown 可改）就到点
    if (++count_down_tick_ >= count_down_max_ticks_) {
      time_percent_ = 1.0;
      count_down_timer_->stop();
      count_down_tick_ = 0;  // 复位，避免下次续接残留值
      if (countdown_silent_) {
        // 外部驱动的视觉倒计时：到点只把显示收掉，**不推进任何东西**——
        // 下一步做什么由脚本决定（见 ShowCountdown）
        countdown_silent_ = false;
        countdown_label_->setVisible(auto_mode_);
        bubble_widget_->update();
        return;
      }
      emit nextClicked();
    } else {
      time_percent_ = static_cast<qreal>(count_down_tick_) /
                      static_cast<qreal>(count_down_max_ticks_);

      // 剩余秒数由"已过去的比例"反推
      const int total_seconds = count_down_max_ticks_ * kCountDownTickMs / 1000;
      const qreal remain = 1.0 - time_percent_;
      const int seconds = static_cast<int>(total_seconds * remain);
      countdown_label_->setText(QString::number(seconds));
      bubble_widget_->update();
    }
  });
}

void TourOverlay::updateUi(TourTopic* const topic,
                                    TourStep* const step, bool canPrev,
                                    bool canNext) {
  //-------------------------------
  // 气泡
  auto index = topic->steps().indexOf(step) + 1;
  setTitle(QString("%1 (%2/%3)")
               .arg(topic->name())
               .arg(index)
               .arg(topic->stepCount()));
  setText(QString("%1.%2").arg(index).arg(step->description()));
  prev_btn_->setEnabled(canPrev);
  next_btn_->setText(canNext ? "下一步" : "完成！");

  //--------------------------------
  // 高亮区域
  pixmap_ = QPixmap();
  // 额外靶**每一步开头都要清**（与上面那行清 pixmap_ 同构）：不清的话上一步的框
  // 会留在屏幕上——第 4 步（三张卡）是三靶、紧随其后的第 5 步是单靶
  extra_rects_.clear();

  // 多靶判定用步骤自己的声明，**不要写 extra_rects_.isEmpty()**：上面刚清过、
  // 锚点那支 visit 完还没收集，那一刻它必然是空表，那个条件恒真
  const bool multi = !step->extraTargets().isEmpty();

  // 空目标步（接线/收尾等纯讲解）：显式清掉上一步的高亮残留——
  // 动画分支不会重置 highlightRect_，否则高亮框滞留在上一个控件上
  const TourStep::Target& target = step->target();
  if (const auto* widget = std::get_if<QWidget*>(&target);
      widget != nullptr && *widget == nullptr) {
    highlightRect_ = QRect();
  }

  if (multi) {
    // 多靶不走动画（设计 D2）：并集难看、N 路并行不值。**stop() 不能省**：
    // 上一步的动画还在跑时（kAnimeDur = 300 ms，连点两下「下一步」就会撞上）
    // 它会经 valueChanged 继续写 anime_highlight_rect_，把下面那行赋值盖掉
    anime_highlight_->stop();
  }

  if (use_anime_ && !multi) {
    // 计算之前设置开始值
    anime_highlight_->setStartValue(highlightRect_);
  } else if (!use_anime_) {
    highlightRect_ = QRect();
  }

  std::visit(
      [this](auto&& target) {
        using T = std::decay_t<decltype(target)>;
        if constexpr (std::is_same_v<T, QWidget*>) {
          if (target) {
            QPoint gPos = target->mapToGlobal(QPoint(0, 0));
            highlightRect_ = QRect(mapFromGlobal(gPos), target->size());
          }
        } else if constexpr (std::is_same_v<T, QRect>) {
          // 得从全局映射坐标系来
          // 且必须假定传入得是全局坐标系
          highlightRect_ = QRect(mapFromGlobal(target.topLeft()),
                                 QSize(target.width(), target.height()));

        } else if constexpr (std::is_same_v<T, QPixmap>) {
          auto center = rect().center();
          highlightRect_ = QRect(center.x() - target.width() / 2,
                                 center.y() - target.height() / 2,
                                 target.width(), target.height());
          pixmap_ = target;
        }
      },
      target);

  // 额外靶：锚点算完之后逐个补上（非空矩形才收，空靶不占位——设计 D6）
  for (const TourStep::Target& extra : step->extraTargets()) {
    const QRect r = RectForExtraTarget(this, extra);
    if (!r.isNull()) {
      extra_rects_.append(r);
    }
  }

  if (multi) {
    // 多靶这一支不走动画，而 drawHighlight / mousePressEvent 读的是
    // anime_highlight_rect_（use_anime_ 恒真、没有 setter），所以这个成员
    // **得自己同步**——不补的话锚点框会停在上一个靶的位置上（幽灵框），
    // 气泡与命中区也跟着错，而且 highlightCount 照样报数，用例抓不住
    anime_highlight_rect_ = highlightRect_;
    update();
  } else if (use_anime_) {
    anime_highlight_->setEndValue(highlightRect_);
    anime_highlight_->start();
  } else {
    update();
  }

  //-------------------------------
  // 气泡位置
  changeBubblePosition();

  // 自动挡。起表这一帧得自己把状态刷到位：复位成"刚开始"（条子满、秒数最大）
  // 并重绘一次，否则第一帧显示的还是上一次留下的空白，要等第一个 tick 才跳变
  if (auto_mode_) {
    time_percent_ = 0.0;
    count_down_tick_ = 0;
    countdown_label_->setText(QString::number(kCountDownSeconds));
    bubble_widget_->update();
    count_down_timer_->start(kCountDownTickMs);
  }
}

void TourOverlay::setIsAutoMode(bool autoMode) {
  auto_mode_ = autoMode;
  countdown_label_->setVisible(auto_mode_);
  // 每次切换都复位计数并停表，避免下次重启从残留值继续。
  count_down_tick_ = 0;
  // 时长与"只显示不推进"都回到自动挡的默认；外部驱动的视觉倒计时
  // （ShowCountdown）用完之后不留痕
  count_down_max_ticks_ = kCountDownMaxTicks;
  countdown_silent_ = false;
  if (!auto_mode_) {
    count_down_timer_->stop();
  }
}

void TourOverlay::UpdateMouseTransparency() {
  // 遮罩子树整体对鼠标穿透：Qt 对该属性的语义是"连同全部子控件"，点击落到
  // 下层真实控件——被高亮的控件、页签、滚动条都可直接操作；点遮罩空白不再
  // 退出（mousePressEvent 不会触发），退出走气泡「跳过」。
  // 两种模式都要求穿透，所以是**或**——谁后设都不能把另一个的条件抹掉
  setAttribute(Qt::WA_TransparentForMouseEvents,
               passive_mode_ || interactive_);
}

void TourOverlay::setInteractive(bool interactive) {
  if (interactive_ == interactive) {
    return;
  }
  interactive_ = interactive;
  UpdateMouseTransparency();
  if (bubble_widget_ == nullptr) {
    return;
  }
  // 气泡要保持可点，不能留在被穿透的子树里：reparent 到遮罩的父级
  // （viewport）。遮罩铺满 viewport 且原点一致，坐标系不变。
  // 实操模式关掉时挂回遮罩，恢复讲解式的父子结构
  if (interactive) {
    // 气泡不能留在被穿透的子树里（那样点不到），也不能挂回宿主窗口——遮罩是
    // 独立顶层窗口，挂在宿主上会被它压在下面，看着像蒙了层阴影。给它自己的
    // 顶层窗口身份，与遮罩平级，靠 raise 压在遮罩之上
    bubble_widget_->setParent(nullptr);
    bubble_widget_->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    bubble_widget_->setAttribute(Qt::WA_TranslucentBackground);
  } else {
    bubble_widget_->setParent(this);
    bubble_widget_->setWindowFlags(Qt::Widget);
  }
  bubble_widget_->raise();
  // setParent 会把控件隐藏，需要显式恢复（此时遮罩已 show）
  bubble_widget_->setVisible(isVisible() && bubble_visible_);
}

void TourOverlay::SetHost(QWidget* host, bool inflate) {
  host_ = host;
  if (host == nullptr) {
    return;
  }

  // 没有 Qt 父子关系，几何得按屏幕坐标自己算：铺满宿主的客户区（可再外扩）
  QRect area(host->mapToGlobal(QPoint(0, 0)), host->size());
  if (inflate) {
    area = area.adjusted(-kHostMargin, -kHostMargin, kHostMargin, kHostMargin);
    // 裁到屏幕可用区：既不会把遮罩伸到屏幕外，也顺带处理了"宿主已占满屏幕"
    const QScreen* screen = QApplication::screenAt(area.center());
    if (screen == nullptr) {
      screen = QApplication::primaryScreen();
    }
    if (screen != nullptr) {
      area = area.intersected(screen->availableGeometry());
    }
  }

  setGeometry(area);
  raise();
}

void TourOverlay::setPassiveMode(bool passive) {
  passive_mode_ = passive;
  // 被动模式是"只显示、不交互"：按钮全隐（没有下一步可点），倒计时一律停
  // ——推进由外部给，不由这里超时决定
  skip_btn_->setVisible(!passive);
  prev_btn_->setVisible(!passive);
  next_btn_->setVisible(!passive);
  // 遮罩是覆盖在宿主上的独立顶层窗口，不收鼠标的话整块区域都会被它吞掉——
  // 被动模式是"只显示"，人得能照常点下面的应用（脚本走 TCP 不受影响，
  // 但脚本卡住时人还得能操作）
  UpdateMouseTransparency();
  if (passive) {
    count_down_timer_->stop();
  }
}

void TourOverlay::setBubbleVisible(bool visible) {
  bubble_visible_ = visible;
  if (bubble_widget_ != nullptr) {
    bubble_widget_->setVisible(visible && isVisible());
  }
}

QPoint TourOverlay::BubbleOrigin() const {
  // 气泡挂在遮罩上时用遮罩局部坐标；实操模式下它被提到独立顶层窗口，坐标
  // 就是屏幕坐标，得补上遮罩自己的屏幕原点
  if (bubble_widget_ == nullptr || bubble_widget_->parentWidget() == this) {
    return QPoint(0, 0);
  }
  return mapToGlobal(QPoint(0, 0));
}

void TourOverlay::ApplyWidgetHighlight(QWidget* target) {
  if (target == nullptr) {
    highlightRect_ = QRect();
    return;
  }
  const QPoint global_pos = target->mapToGlobal(QPoint(0, 0));
  highlightRect_ = QRect(mapFromGlobal(global_pos), target->size());
}

void TourOverlay::showHighlight(QWidget* target, const QString& title,
                                         const QString& text,
                                         const QList<QWidget*>& extras) {
  if (target == nullptr) {
    return;
  }

  // 目标可能在别的顶层窗口里（比如模态对话框）：遮罩得跟过去，否则它铺在
  // 主窗口上、根本盖不到那个控件
  QWidget* target_window = target->window();
  if (target_window != nullptr) {
    // 目标就是窗口本身时让遮罩外扩，否则高亮会盖满整块遮罩、阴影被掏空
    SetHost(target_window, /*inflate=*/target == target_window);
  }

  ApplyWidgetHighlight(target);
  // **无条件按参数赋值**：给了 extras 就是它们，没给就是空表（不清的话上一步
  // 的多靶框会留在屏幕上——两步之间不一定有 hideHighlight）
  extra_rects_.clear();
  for (QWidget* extra : extras) {
    const QRect r = RectForExtraTarget(this, TourStep::Target(extra));
    if (!r.isNull()) {
      extra_rects_.append(r);
    }
  }
  FinishShowHighlight(title, text);
}

void TourOverlay::showHighlightRect(
    QWidget* host, const QRect& global_rect, const QString& title,
    const QString& text, const QList<QRect>& extras) {
  if (host == nullptr || global_rect.isNull()) {
    return;
  }
  // 矩形已经是全局坐标，遮罩铺到它所在的窗口上就够（不外扩：矩形是别人算好的）
  SetHost(host, /*inflate=*/false);
  highlightRect_ = QRect(mapFromGlobal(global_rect.topLeft()),
                         global_rect.size());
  // 同 showHighlight：无条件赋值，空参数 = 空表
  extra_rects_.clear();
  for (const QRect& extra : extras) {
    const QRect r = RectForExtraTarget(this, TourStep::Target(extra));
    if (!r.isNull()) {
      extra_rects_.append(r);
    }
  }
  FinishShowHighlight(title, text);
}

void TourOverlay::FinishShowHighlight(const QString& title,
                                               const QString& text) {
  anime_highlight_rect_ = highlightRect_;
  pixmap_ = QPixmap();

  setTitle(title);
  setText(text);
  // 外部驱动：不走动画、也不碰倒计时（被动模式下它本来就是停的）。
  // 要倒计时的场合由桥接另外调 ShowCountdown
  changeBubblePosition();
  update();

  show();
  raise();
}

void TourOverlay::ShowCountdown(int seconds) {
  if (seconds <= 0 || count_down_timer_ == nullptr) {
    return;
  }
  countdown_silent_ = true;
  count_down_max_ticks_ = seconds * 1000 / kCountDownTickMs;
  time_percent_ = 0.0;
  count_down_tick_ = 0;
  // 起表这一帧自己把状态刷到位，否则第一帧显示的还是上一次留下的空白
  countdown_label_->setText(QString::number(seconds));
  countdown_label_->setVisible(true);
  bubble_widget_->update();
  count_down_timer_->start(kCountDownTickMs);
}

void TourOverlay::clearHighlight() {
  highlightRect_ = QRect();
  // 额外靶也要清：clearHighlight 是"把屏幕上的框全撤掉"，只清锚点会留下几个
  // 没有依托的孤框
  extra_rects_.clear();
  anime_highlight_rect_ = QRect();
  pixmap_ = QPixmap();
  hide();
}

int TourOverlay::highlightCount() const {
  // 锚点 + 额外靶。空锚点（纯讲解步）不算——那一步屏幕上本来就没有框
  return (highlightRect_.isEmpty() ? 0 : 1) + extra_rects_.size();
}

void TourOverlay::showEvent(QShowEvent* event) {
  // 气泡的显隐必须单独同步，两种父级都躲不掉：
  //   实操模式——气泡挂在 viewport 上，不随遮罩的父子关系显隐
  //   讲解模式——气泡挂在遮罩上，但 hideEvent 显式藏过它，而 Qt 里被显式
  //             隐藏的子控件不会随父控件 show() 回来（showChildren 会跳过
  //             isHidden() 的子控件），于是第二遍只剩遮罩、气泡不见了
  //
  // raise 不能省：execute() 每次都会把遮罩 setParent 回 viewport（插入即
  // 最上层），而 setInteractive 在状态未变时早退、不会重排层级——漏了
  // raise，半透明遮罩就会叠在气泡上（点击穿透所以按钮还能点，但看着
  // 像被阴影盖住，第二次运行起偶发）
  if (bubble_widget_ != nullptr) {
    bubble_widget_->setVisible(bubble_visible_);
    bubble_widget_->raise();
  }
  QWidget::showEvent(event);
}

void TourOverlay::hideEvent(QHideEvent* event) {
  // stop() 只藏遮罩；气泡若挂在 viewport 上会残留成"幽灵气泡"
  if (bubble_widget_ != nullptr) {
    bubble_widget_->hide();
  }
  QWidget::hideEvent(event);
}

void TourOverlay::drawCountDownOnBubble(QPainter* painter) {
  // 自动挡与"外部驱动的视觉倒计时"都要画这条进度
  if (!auto_mode_ && !countdown_silent_) {
    return;
  }

  auto& p = *painter;

  int margin = kLayoutMargin - 9;
  QRectF rect = bubble_widget_->rect().marginsRemoved(
      QMargins(margin, margin, margin, margin));

  rect.setWidth(rect.width() * (1 - time_percent_));

  QPainterPath path;
  path.addRoundedRect(rect, cornerRadius_, cornerRadius_);
  // 倒计时条用 accent 淡色（两主题统一降低深浅）
  QColor bar = accentColor();
  bar.setAlpha(140);
  p.fillPath(path, QBrush(bar));
}

void TourOverlay::drawBubble(QPainter* painter) {
  auto& p = *painter;

  const int margin = kLayoutMargin - 9;
  const QRectF bubble_rect = bubble_widget_->rect().marginsRemoved(
      QMargins(margin, margin, margin, margin));

  QPainterPath path;
  path.addRoundedRect(bubble_rect, cornerRadius_, cornerRadius_);
  // 三角箭头只在有高亮矩形依托时绘制；纯讲解步（气泡居中）没有指向
  // 对象，就是一个干净的圆角矩形
  if (!highlightRect_.isEmpty()) {
    path.addPath(ArrowPath());
  }
  p.fillPath(path, QBrush(bubbleColor()));
}

QPainterPath TourOverlay::ArrowPath() const {
  // 三角箭头：底边贴气泡面向目标一侧的中点（半宽 = margin），尖端顶到
  // 控件边缘。position_ 记录的是气泡在目标的哪一侧，箭头画在相反一侧
  const QRectF area(bubble_widget_->rect());
  const qreal cx = area.center().x();
  const qreal cy = area.center().y();
  const qreal m = kLayoutMargin - 9;

  QPointF base_a;
  QPointF base_b;
  QPointF tip;
  switch (position_) {
    case Position::Left:  // 气泡在目标的左，箭头朝右
      tip = QPointF(area.right(), cy);
      base_a = QPointF(area.right() - m, cy - m);
      base_b = QPointF(area.right() - m, cy + m);
      break;
    case Position::Top:  // 气泡在目标的上，箭头朝下
      tip = QPointF(cx, area.bottom());
      base_a = QPointF(cx - m, area.bottom() - m);
      base_b = QPointF(cx + m, area.bottom() - m);
      break;
    case Position::Right:  // 气泡在目标的右，箭头朝左
      tip = QPointF(area.left(), cy);
      base_a = QPointF(area.left() + m, cy - m);
      base_b = QPointF(area.left() + m, cy + m);
      break;
    case Position::Bottom:  // 气泡在目标的下，箭头朝上
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

void TourOverlay::drawHighlight(QPainter* painter) {
  QRect target_rect = use_anime_ ? anime_highlight_rect_ : highlightRect_;

  auto& p = *painter;
  if (target_rect.isNull() && extra_rects_.isEmpty()) {
    p.fillRect(rect(), maskColor());
    return;
  }

  // 锚点 + 额外靶一起挖（《一步多靶设计》§3.2）。锚点可能为空（纯讲解步），
  // 那一步只画额外靶
  QList<QRect> rects;
  if (!target_rect.isNull()) {
    rects.append(target_rect);
  }
  for (const QRect& extra : extra_rects_) {
    rects.append(extra);
  }

  QPainterPath fullPath;
  fullPath.addRect(rect());

  QPainterPath highlightPath;
  // **WindingFill 不能省**：路径的集合运算把路径当"区域"，而区域由 fill rule
  // 决定——默认的 OddEvenFill 下，两个重叠的圆角矩形在重叠区是"偶数次覆盖"、
  // 算作洞外，相减之后那块仍被遮罩涂黑。相邻两个框必然重叠（卡片的
  // QHBoxLayout 间距 8px，而框往外扩 margin_ = 10），不改就是一条条黑竖条
  highlightPath.setFillRule(Qt::WindingFill);
  for (const QRect& r : rects) {
    highlightPath.addRoundedRect(
        r.adjusted(-margin_, -margin_, margin_, margin_), 8, 8);
  }

  // 通过挖孔的方式突出高亮
  QPainterPath maskPath = fullPath - highlightPath;
  p.fillPath(maskPath, maskColor());

  // 绘制一个虚线外框（accent 色）
  p.save();
  p.setPen(QPen(accentColor(), borderWidth_, Qt::DashLine, Qt::RoundCap,
                Qt::RoundJoin));
  for (const QRect& r : rects) {
    p.drawRoundedRect(r.adjusted(-margin_, -margin_, margin_, margin_), 8, 8);
  }
  p.restore();

  // 如果有图片，在区域内绘制图片（图片只可能是锚点：多靶不支持图片，见 D4）
  if (!pixmap_.isNull() && !target_rect.isNull()) {
    p.drawPixmap(target_rect, pixmap_, pixmap_.rect());
  }
}

void TourOverlay::mousePressEvent(QMouseEvent* event) {
  // 被动模式：由外部驱动，点哪儿都不该把它关掉
  if (passive_mode_) {
    return;
  }

  // 高亮目标区域：引导期间目标不响应鼠标，点击仅拦截不退出。
  //
  // **逐个矩形判，不合并并集**（设计 D3）：多靶时两块之间那块空白也算"点在
  // 目标上"的话，遮罩就永远关不掉了（讲解式主题点空白 = 退出）
  QRect target_rect = use_anime_ ? anime_highlight_rect_ : highlightRect_;
  QList<QRect> hit_rects;
  if (!target_rect.isNull()) {
    hit_rects.append(target_rect);
  }
  for (const QRect& extra : extra_rects_) {
    hit_rects.append(extra);
  }
  for (const QRect& r : hit_rects) {
    if (r.adjusted(-margin_, -margin_, margin_, margin_)
            .contains(event->pos())) {
      return;
    }
  }

  // 遮罩空白：退出引导。
  count_down_timer_->stop();
  emit skipClicked();
  QWidget::mousePressEvent(event);
}

void TourOverlay::changeBubblePosition() {
  int spacing = kLayoutMargin - 15;

  // 空目标步：没有高亮矩形可依托，方向评估无意义，气泡直接在画面居中
  // （原先会落进钳位逻辑被按到左上角）
  if (highlightRect_.isEmpty()) {
    const QSize bubble_size = bubble_widget_->size();
    const int centered_x = (width() - bubble_size.width()) / 2;
    const int centered_y = (height() - bubble_size.height()) / 2;
    const QPoint centered = BubbleOrigin() + QPoint(centered_x, centered_y);
    if (!use_anime_) {
      bubble_widget_->move(centered);
    } else {
      anime_bubble_->setStartValue(bubble_widget_->pos());
      anime_bubble_->setEndValue(centered);
      anime_bubble_->start();
    }
    return;
  }

  DirectionInfo infoLeft = evaluateDirectionSpace(Position::Left);
  DirectionInfo infoRight = evaluateDirectionSpace(Position::Right);
  DirectionInfo infoTop = evaluateDirectionSpace(Position::Top);
  DirectionInfo infoBottom = evaluateDirectionSpace(Position::Bottom);

  Position bestPos = Position::Right;

  if (infoLeft.canPlace && infoRight.canPlace) {
    if (infoLeft.spaceWidth > infoRight.spaceWidth) {
      bestPos = Position::Left;
    } else {
      bestPos = Position::Right;
    }
  } else if (infoLeft.canPlace) {
    bestPos = Position::Left;
  } else if (infoRight.canPlace) {
    bestPos = Position::Right;
  } else if (infoTop.canPlace && infoBottom.canPlace) {
    if (infoTop.spaceHeight > infoBottom.spaceHeight) {
      bestPos = Position::Top;
    } else {
      bestPos = Position::Bottom;
    }
  } else if (infoTop.canPlace) {
    bestPos = Position::Top;
  } else if (infoBottom.canPlace) {
    bestPos = Position::Bottom;
  } else {
    int maxSpace = qMax(qMax(infoLeft.spaceWidth, infoRight.spaceWidth),
                        qMax(infoTop.spaceHeight, infoBottom.spaceHeight));
    if (maxSpace == infoLeft.spaceWidth) {
      bestPos = Position::Left;
    } else if (maxSpace == infoRight.spaceWidth) {
      bestPos = Position::Right;
    } else if (maxSpace == infoTop.spaceHeight) {
      bestPos = Position::Top;
    } else {
      bestPos = Position::Bottom;
    }
  }

  int x = 0, y = 0;

  QRect targetRect = highlightRect_;
  QSize viewportSize = this->size();
  QSize bubbleSize = bubble_widget_->size();
  switch (bestPos) {
    case Position::Left:
      x = targetRect.left() - bubbleSize.width() - spacing;
      y = targetRect.center().y() - bubbleSize.height() / 2;
      break;
    case Position::Right:
      x = targetRect.right() + spacing;
      y = targetRect.center().y() - bubbleSize.height() / 2;
      break;
    case Position::Top:
      x = targetRect.center().x() - bubbleSize.width() / 2;
      y = targetRect.top() - bubbleSize.height() - spacing;
      break;
    case Position::Bottom:
      x = targetRect.center().x() - bubbleSize.width() / 2;
      y = targetRect.bottom() + spacing;
      break;
    default:
      break;
  }

  const int margin = 20;
  x = qMax(margin, qMin(x, viewportSize.width() - bubbleSize.width() - margin));
  y = qMax(margin,
           qMin(y, viewportSize.height() - bubbleSize.height() - margin));

  const QPoint target_pos = BubbleOrigin() + QPoint(x, y);
  if (!use_anime_) {
    bubble_widget_->move(target_pos);
  } else {
    anime_bubble_->setStartValue(bubble_widget_->pos());
    anime_bubble_->setEndValue(target_pos);
    anime_bubble_->start();
  }
  position_ = bestPos;
}

TourOverlay::DirectionInfo
TourOverlay::evaluateDirectionSpace(Position pos) {
  const int spacing = 15;
  DirectionInfo info;

  QRect targetRect = highlightRect_;
  QSize viewportSize = this->size();
  QSize bubbleSize = bubble_widget_->size();

  switch (pos) {
    case Left: {
      info.spaceWidth = targetRect.left();
      info.spaceHeight = viewportSize.height();

      if (bubbleSize.height() <= viewportSize.height() - spacing * 2 &&
          bubbleSize.width() + spacing <= targetRect.left()) {
        info.canPlace = true;
      }
    } break;
    case Top: {
      info.spaceWidth = viewportSize.width();
      info.spaceHeight = targetRect.top();

      if (bubbleSize.width() <= viewportSize.width() - spacing * 2 &&
          bubbleSize.height() + spacing <= targetRect.top()) {
        info.canPlace = true;
      }
    } break;
    case Right: {
      info.spaceWidth = viewportSize.width() - targetRect.right();
      info.spaceHeight = viewportSize.height();

      if (bubbleSize.height() <= viewportSize.height() - spacing * 2 &&
          bubbleSize.width() + spacing <=
              viewportSize.width() - targetRect.right()) {
        info.canPlace = true;
      }
    } break;
    case Bottom: {
      info.spaceWidth = viewportSize.width();
      info.spaceHeight = viewportSize.height() - targetRect.bottom();

      if (bubbleSize.width() <= viewportSize.width() - spacing * 2 &&
          bubbleSize.height() + spacing <=
              viewportSize.height() - targetRect.bottom()) {
        info.canPlace = true;
      }
    } break;
    default:
      break;
  }
  return info;
}

void TourOverlay::paintEvent(QPaintEvent* event) {
  QWidget::paintEvent(event);

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  drawHighlight(&p);
}

QColor TourOverlay::maskColor() const {
  // 遮罩：半透明黑。alpha 定在这里，深浅要调只改这一个数
  const int alpha = 128;
  return QColor(0, 0, 0, alpha);
}

QColor TourOverlay::bubbleColor() const {
  QColor color = QColor(0xF0, 0xF0, 0xF0);
  color.setAlpha(245);
  return color;
}

QColor TourOverlay::accentColor() const { return QColor("#007ACC"); }
