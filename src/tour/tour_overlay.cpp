#include "tour_overlay.h"

#include <variant>

#include <QApplication>
#include <QHideEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QShowEvent>

#include "tour_bubble_view.h"
#include "tour_catalog.h"
#include "tour_countdown.h"
#include "tour_overlay_mask.h"

namespace {

// 控件 → 遮罩局部矩形。锚点（updateUi 与 showHighlight*）与额外靶共用它——
// "两处算出不同的框"正是要避免的事
QRect RectInOverlay(const QWidget* overlay, const QWidget* widget) {
  if (widget == nullptr) {
    return QRect();
  }
  return QRect(overlay->mapFromGlobal(widget->mapToGlobal(QPoint(0, 0))),
               widget->size());
}

// 靶 → 遮罩局部矩形（《一步多靶设计》§3.2）。
//
// 只有两种靶，所以这里**没有"图片怎么办"这一支**——原先那个 qWarning（"额外靶
// 不支持图片"）是靠注释约定的，现在 SplashImage 压根进不了 Target，类型挡住了
QRect RectInOverlay(const QWidget* overlay, const Target& target) {
  return std::visit(
      [overlay](auto&& t) -> QRect {
        using T = std::decay_t<decltype(t)>;
        if constexpr (std::is_same_v<T, WidgetTarget>) {
          return RectInOverlay(overlay, t.widget);
        } else {
          // GlobalRect：全局坐标 → 遮罩局部
          if (t.rect.isNull()) {
            return QRect();
          }
          return QRect(overlay->mapFromGlobal(t.rect.topLeft()), t.rect.size());
        }
      },
      target);
}

}  // namespace

TourOverlay::TourOverlay(QWidget* parent)
    : QWidget(parent),
      mask_(new OverlayMask(this)),
      countdown_(new TourCountdown(this)),
      bubble_(nullptr) {
  // 独立顶层窗口（不设 Qt 父子，宿主由 setHost 记）：无边框、不进任务栏，
  // 半透明让宿主内容透出（遮罩底色自绘）
  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
  // 顶层窗口也要有名字：外部工具对没名字的顶层只能编"类名-序号"，
  // 连"遮罩显没显示"都没法查
  setObjectName(QStringLiteral("tourOverlay"));
  setAttribute(Qt::WA_TranslucentBackground);
  setAutoFillBackground(false);
  initUi();
  initSignals();
}

TourOverlay::~TourOverlay() {
  // 气泡可能已经被 setParent(nullptr) 摘出了父子链（实操模式），不再有人替
  // 我们删它；无条件删一次，两种情况都对
  delete bubble_;
  bubble_ = nullptr;
}

void TourOverlay::initUi() {
  bubble_ = new BubbleView(mask_, countdown_, this);
}

void TourOverlay::initSignals() {
  // 遮罩每换一次高亮、动画每走一帧都要重画
  connect(mask_, &OverlayMask::repaintRequested, this, [this] { update(); });

  // 气泡上那三个按钮：先停表，再往外发。停表不能省——不然自动挡那 5 秒走到头
  // 还会再翻一页
  connect(bubble_, &BubbleView::nextClicked, this, [this] {
    countdown_->stop();
    emit nextClicked();
  });
  connect(bubble_, &BubbleView::prevClicked, this, [this] {
    countdown_->stop();
    emit prevClicked();
  });
  connect(bubble_, &BubbleView::skipClicked, this, [this] {
    countdown_->stop();
    emit skipClicked();
  });

  // 自动挡走到头 = 该翻页了。静默那趟走到头不发这个信号（显示收掉由
  // TourCountdown::changed 负责），下一步做什么由脚本决定
  connect(countdown_, &TourCountdown::finished, this,
          &TourOverlay::nextClicked);
}

void TourOverlay::updateUi(TourTopic* const topic, TourStep* const step,
                           bool canPrev, bool canNext) {
  //-------------------------------
  // 气泡
  auto index = topic->steps().indexOf(step) + 1;
  bubble_->setTitle(QString("%1 (%2/%3)")
                        .arg(topic->name())
                        .arg(index)
                        .arg(topic->stepCount()));
  bubble_->setText(QString("%1.%2").arg(index).arg(step->description()));
  bubble_->setNavigation(canPrev, canNext);

  //--------------------------------
  // 高亮区域
  // 锚点与额外靶**每一步都从零算**（不这么写，上一步的框就留在屏幕上：第 4 步
  // 那三张卡是三靶、紧随其后的第 5 步是单靶）。空目标步（接线/收尾等纯讲解）
  // 天然停在空锚点上，气泡居中
  QRect anchor;
  QList<QRect> extras;
  QPixmap pixmap;

  std::visit(
      [&](auto&& target) {
        using T = std::decay_t<decltype(target)>;
        if constexpr (std::is_same_v<T, WidgetTarget>) {
          anchor = RectInOverlay(this, target.widget);
        } else if constexpr (std::is_same_v<T, GlobalRect>) {
          // GlobalRect 的坐标口径由类型担保（见 tour_catalog.h），这里不必再
          // 假定什么——映射到遮罩坐标系即可
          anchor = RectInOverlay(this, Target{target});
        } else if constexpr (std::is_same_v<T, SplashImage>) {
          // 图示步：不挖洞，把图贴在遮罩中央，锚点就是图占的那块
          const QPoint center = rect().center();
          anchor = QRect(center.x() - target.pixmap.width() / 2,
                         center.y() - target.pixmap.height() / 2,
                         target.pixmap.width(), target.pixmap.height());
          pixmap = target.pixmap;
        }
      },
      step->content());

  // 额外靶：锚点算完之后逐个补上（非空矩形才收，空靶不占位——设计 D6）
  for (const Target& extra : step->extraTargets()) {
    const QRect r = RectInOverlay(this, extra);
    if (!r.isNull()) {
      extras.append(r);
    }
  }

  mask_->setPixmap(pixmap);
  // 有没有额外靶决定要不要动画（多靶并集难看，见 OverlayMask::setHighlight）
  mask_->setHighlight(anchor, extras, /*animated=*/true);

  //-------------------------------
  // 气泡位置
  changeBubblePosition();

  // 自动挡：本步起表（起表那一帧的状态刷新在 TourCountdown 里）
  if (countdown_->autoMode()) {
    countdown_->startAuto();
  }
}

void TourOverlay::setIsAutoMode(bool autoMode) {
  // 倒计时的复位、停表、显隐都在它自己那里（见 TourCountdown::setAutoMode）
  countdown_->setAutoMode(autoMode);
}

void TourOverlay::updateMouseTransparency() {
  // 遮罩子树整体对鼠标穿透：Qt 对该属性的语义是"连同全部子控件"，点击落到
  // 下层真实控件——被高亮的控件、页签、滚动条都可直接操作；点遮罩空白不再
  // 退出（mousePressEvent 不会触发），退出走气泡「跳过」。
  // 两种模式都要求穿透，所以是**或**——谁后设都不能把另一个的条件抹掉
  setAttribute(Qt::WA_TransparentForMouseEvents, passive_mode_ || interactive_);
}

void TourOverlay::setInteractive(bool interactive) {
  if (interactive_ == interactive) {
    return;
  }
  interactive_ = interactive;
  updateMouseTransparency();
  // 气泡要保持可点，不能留在被穿透的子树里；挂法与坐标原点都由气泡自己收口
  if (interactive) {
    bubble_->detachToTopLevel();
  } else {
    bubble_->attachToOverlay();
  }
}

void TourOverlay::setHost(QWidget* host, bool inflate) {
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
  bubble_->setButtonsVisible(!passive);
  // 遮罩是覆盖在宿主上的独立顶层窗口，不收鼠标的话整块区域都会被它吞掉——
  // 被动模式是"只显示"，人得能照常点下面的应用（脚本走 TCP 不受影响，
  // 但脚本卡住时人还得能操作）
  updateMouseTransparency();
  if (passive) {
    countdown_->stop();
  }
}

void TourOverlay::setBubbleVisible(bool visible) {
  bubble_->setBubbleVisible(visible);
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
    setHost(target_window, /*inflate=*/target == target_window);
  }

  // 额外靶**无条件按参数赋值**：给了 extras 就是它们，没给就是空表（不清的话
  // 上一步的多靶框会留在屏幕上——两步之间不一定有 hideHighlight）
  QList<QRect> extra_rects;
  for (QWidget* extra : extras) {
    const QRect r = RectInOverlay(this, Target(WidgetTarget{extra}));
    if (!r.isNull()) {
      extra_rects.append(r);
    }
  }
  // 外部驱动：不走动画、也不碰倒计时
  mask_->setHighlight(RectInOverlay(this, target), extra_rects,
                      /*animated=*/false);
  finishShowHighlight(title, text);
}

void TourOverlay::showHighlightRect(QWidget* host, const QRect& global_rect,
                                    const QString& title, const QString& text,
                                    const QList<QRect>& extras) {
  if (host == nullptr || global_rect.isNull()) {
    return;
  }
  // 矩形已经是全局坐标，遮罩铺到它所在的窗口上就够（不外扩：矩形是别人算好的）
  setHost(host, /*inflate=*/false);

  // 同 showHighlight：无条件赋值，空参数 = 空表
  QList<QRect> extra_rects;
  for (const QRect& extra : extras) {
    const QRect r = RectInOverlay(this, Target(GlobalRect{extra}));
    if (!r.isNull()) {
      extra_rects.append(r);
    }
  }
  mask_->setHighlight(
      QRect(mapFromGlobal(global_rect.topLeft()), global_rect.size()),
      extra_rects, /*animated=*/false);
  finishShowHighlight(title, text);
}

void TourOverlay::finishShowHighlight(const QString& title,
                                      const QString& text) {
  mask_->setPixmap(QPixmap());

  bubble_->setTitle(title);
  bubble_->setText(text);
  // 外部驱动：不走动画、也不碰倒计时（被动模式下它本来就是停的）。
  // 要倒计时的场合由桥接另外调 showCountdown
  changeBubblePosition();
  update();

  show();
  raise();
}

void TourOverlay::showCountdown(int seconds) {
  // 走"静默"那一趟：只显示、不推进
  countdown_->startSilent(seconds);
}

void TourOverlay::clearHighlight() {
  // 一处清（原先是三个成员各写一句，漏一个就是"屏幕上留着没有依托的孤框"）
  mask_->clear();
  hide();
}

QRect TourOverlay::highlightRect() const { return mask_->anchor(); }

int TourOverlay::highlightCount() const {
  // 锚点 + 额外靶。空锚点（纯讲解步）不算——那一步屏幕上本来就没有框
  return mask_->count();
}

void TourOverlay::showEvent(QShowEvent* event) {
  // 气泡的显隐与层级要单独同步（两种挂法都躲不掉，见
  // BubbleView::syncVisibility）
  bubble_->syncVisibility();
  QWidget::showEvent(event);
}

void TourOverlay::hideEvent(QHideEvent* event) {
  // stop() 只藏遮罩；气泡若挂在 viewport 上会残留成"幽灵气泡"
  bubble_->setVisible(false);
  QWidget::hideEvent(event);
}

void TourOverlay::mousePressEvent(QMouseEvent* event) {
  // 被动模式：由外部驱动，点哪儿都不该把它关掉
  if (passive_mode_) {
    return;
  }

  // 高亮目标区域：引导期间目标不响应鼠标，点击仅拦截不退出
  // （逐个矩形判，不合并并集——见 OverlayMask::hitTarget）
  if (mask_->hitTarget(event->pos())) {
    return;
  }

  // 遮罩空白：退出引导。
  countdown_->stop();
  emit skipClicked();
  QWidget::mousePressEvent(event);
}

void TourOverlay::changeBubblePosition() {
  // 定位算法与坐标原点都在气泡那边（见 BubbleView::followAnchor）
  bubble_->followAnchor(size());
}

void TourOverlay::paintEvent(QPaintEvent* event) {
  QWidget::paintEvent(event);

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  mask_->paint(&p, rect());
}
