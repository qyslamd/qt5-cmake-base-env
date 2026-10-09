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
#include "tour_mask_geometry.h"
#include "tour_overlay_mask.h"

namespace {

// 控件 → 遮罩局部矩形。锚点与额外靶共用它，免得两处算出不同的框
QRect RectInOverlay(const QWidget* overlay, const QWidget* widget) {
  if (widget == nullptr) {
    return QRect();
  }
  return QRect(overlay->mapFromGlobal(widget->mapToGlobal(QPoint(0, 0))),
               widget->size());
}

// 靶 → 遮罩局部矩形。只有两种靶：SplashImage 进不了 Target，所以这里没有
// "图片怎么办"这一支（类型挡住，不是注释约定）
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
      mask_(new TourOverlayMask(this)),
      countdown_(new TourCountdown(this)),
      bubble_(nullptr) {
  // 独立顶层窗口（无 Qt 父子，宿主由 setHost 记）：无边框、不进任务栏，
  // 半透明让宿主内容透出（遮罩底色自绘）
  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
  // 顶层也得有名字：外部工具对没名字的顶层只能编"类名-序号"。
  // 这个名字是 funq 用例的寻址锚点，改不得
  setObjectName(QStringLiteral("tourOverlay"));
  setAttribute(Qt::WA_TranslucentBackground);
  setAutoFillBackground(false);
  initUi();
  initSignals();
}

TourOverlay::~TourOverlay() {
  // 气泡可能已被 setParent(nullptr) 摘出父子链（实操模式），不再有人替我们
  // 删它；无条件删一次，两种情况都对
  delete bubble_;
  bubble_ = nullptr;
}

void TourOverlay::initUi() { bubble_ = new TourBubbleView(countdown_, this); }

void TourOverlay::initSignals() {
  // 遮罩每换一次高亮、动画每走一帧都要重画
  connect(mask_, &TourOverlayMask::repaintRequested, this,
          [this] { update(); });

  // 按钮点击：先停表再往外发，不然自动挡走到头还会再翻一页
  connect(bubble_, &TourBubbleView::nextClicked, this, [this] {
    countdown_->stop();
    emit nextClicked();
  });
  connect(bubble_, &TourBubbleView::prevClicked, this, [this] {
    countdown_->stop();
    emit prevClicked();
  });
  connect(bubble_, &TourBubbleView::skipClicked, this, [this] {
    countdown_->stop();
    emit skipClicked();
  });

  // 自动挡走到头 = 该翻页了。静默那趟不发这个信号（只收显示），
  // 下一步做什么由脚本决定
  connect(countdown_, &TourCountdown::finished, this,
          &TourOverlay::nextClicked);
}

void TourOverlay::updateUi(TourTopic* const topic, TourStep* const step,
                           bool can_prev, bool can_next) {
  //-------------------------------
  // 气泡
  auto index = topic->steps().indexOf(step) + 1;
  bubble_->setTitle(QString("%1 (%2/%3)")
                        .arg(topic->name())
                        .arg(index)
                        .arg(topic->stepCount()));
  bubble_->setText(QString("%1.%2").arg(index).arg(step->description()));
  bubble_->setNavigation(can_prev, can_next);

  //--------------------------------
  // 高亮区域
  // 锚点与额外靶**每一步都从零算**：累计的话上一步的框会留在屏幕上
  // （空目标步就该停在空锚点上，气泡居中）
  QRect anchor;
  QList<QRect> extras;
  QPixmap pixmap;

  std::visit(
      [&](auto&& target) {
        using T = std::decay_t<decltype(target)>;
        if constexpr (std::is_same_v<T, WidgetTarget>) {
          anchor = RectInOverlay(this, target.widget);
        } else if constexpr (std::is_same_v<T, GlobalRect>) {
          // 坐标口径由类型担保（见 tour_catalog.h），映射进来即可
          anchor = RectInOverlay(this, Target{target});
        } else if constexpr (std::is_same_v<T, SplashImage>) {
          // 图示步：不挖洞，图贴遮罩中央，锚点就是图占的那块
          const QPoint center = rect().center();
          anchor = QRect(center.x() - target.pixmap.width() / 2,
                         center.y() - target.pixmap.height() / 2,
                         target.pixmap.width(), target.pixmap.height());
          pixmap = target.pixmap;
        }
      },
      step->content());

  // 额外靶在锚点之后补上；空矩形不占位
  for (const Target& extra : step->extraTargets()) {
    const QRect r = RectInOverlay(this, extra);
    if (!r.isNull()) {
      extras.append(r);
    }
  }

  mask_->setPixmap(pixmap);
  // 要不要动画由"有没有额外靶"决定，见 TourOverlayMask::setHighlight
  mask_->setHighlight(anchor, extras, /*animated=*/true);

  changeBubblePosition();

  // 自动挡：本步起表（起表那一帧的状态刷新在倒计时里）
  if (countdown_->autoMode()) {
    countdown_->startAuto();
  }
}

void TourOverlay::setIsAutoMode(bool auto_mode) {
  // 纯转发，**但要留着**：控制器只该知道"这一趟是不是自动挡"，不该知道倒计时
  // 是单独一个类（那是演示层的内部结构）。倒计时的复位、停表、显隐都在它自己
  // 那里（见 TourCountdown::setAutoMode）
  countdown_->setAutoMode(auto_mode);
}

void TourOverlay::updateMouseTransparency() {
  // 该属性的语义是"连同全部子控件"穿透：点击落到下层真实控件（高亮的控件、
  // 页签、滚动条都能操作），点遮罩空白也不再触发 mousePressEvent。
  // 两种模式都要求穿透，所以是**或**
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
  }

  // 宿主比"最小视口"还小就把遮罩撑大（见 PlaceMask）：气泡的视口就是遮罩的
  // 尺寸，而登录框那种 460×370 的宿主上四向都放不下气泡，只剩"压着目标"。
  // 最小视口从气泡自己的最小尺寸推：**两侧各放得下一个气泡 + 一个钳位边距**
  const QSize min_viewport =
      bubble_->minimumSize() * 2 +
      QSize(2 * kBubbleClampMargin, 2 * kBubbleClampMargin);
  // 裁到屏幕可用区：不会把遮罩伸到屏幕外，也顺带处理"宿主已占满屏幕"
  const QScreen* screen = QApplication::screenAt(area.center());
  if (screen == nullptr) {
    screen = QApplication::primaryScreen();
  }
  area = PlaceMask(area, min_viewport,
                   screen == nullptr ? QRect() : screen->availableGeometry());

  setGeometry(area);
  raise();
}

void TourOverlay::setPassiveMode(bool passive) {
  passive_mode_ = passive;
  // "只显示、不交互"：按钮全隐，倒计时一律停——推进由外部给
  bubble_->setButtonsVisible(!passive);
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

  // 目标可能在别的顶层窗口里（比如模态框）：遮罩得跟过去，否则盖不到那个控件
  QWidget* target_window = target->window();
  if (target_window != nullptr) {
    // 目标就是窗口本身时外扩，否则高亮会盖满整块遮罩、阴影被掏空
    setHost(target_window, /*inflate=*/target == target_window);
  }

  // 额外靶**无条件按参数赋值**：不传就是空表，否则上一步的框会留在屏幕上
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
  // 矩形已经是全局坐标，遮罩铺到它所在的窗口上就够（不外扩：它是别人算好的）
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
  // 外部驱动：不走动画、也不碰倒计时。要倒计时的场合由桥接另外调 showCountdown
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
  // 一处清：三个成员分着清，漏一个屏幕上就留着孤框
  mask_->clear();
  hide();
}

QRect TourOverlay::highlightRect() const { return mask_->anchor(); }

int TourOverlay::highlightCount() const {
  // 锚点 + 额外靶；空锚点（纯讲解步）不算，那一步根本没有框
  return mask_->count();
}

void TourOverlay::showEvent(QShowEvent* event) {
  // 气泡的显隐与层级要单独同步（两种挂法都躲不掉，见 syncVisibility）
  bubble_->syncVisibility();
  QWidget::showEvent(event);
}

void TourOverlay::hideEvent(QHideEvent* event) {
  // 只藏遮罩的话，挂在 viewport 上的气泡会残留成"幽灵气泡"
  bubble_->setVisible(false);
  QWidget::hideEvent(event);
}

void TourOverlay::mousePressEvent(QMouseEvent* event) {
  // 被动模式由外部驱动，点哪儿都不该把它关掉
  if (passive_mode_) {
    return;
  }

  // 点在靶上：只拦截不退出（逐个矩形判，见 TourOverlayMask::hitTarget）
  if (mask_->hitTarget(event->pos())) {
    return;
  }

  // 点空白 = 退出引导
  countdown_->stop();
  emit skipClicked();
  QWidget::mousePressEvent(event);
}

void TourOverlay::changeBubblePosition() {
  // 定位算法与坐标原点都在气泡那边（见 followAnchor）；靶由这里按次递过去
  bubble_->followAnchor(mask_->anchor(), size());
}

void TourOverlay::paintEvent(QPaintEvent* event) {
  QWidget::paintEvent(event);

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  mask_->paint(&p, rect());
}
