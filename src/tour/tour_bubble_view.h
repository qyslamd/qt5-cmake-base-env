#ifndef FILE_TOUR_BUBBLE_VIEW_H_
#define FILE_TOUR_BUBBLE_VIEW_H_

#include <QWidget>

#include "tour_bubble_placer.h"

class QLabel;
class QPainter;
class QPainterPath;
class QPropertyAnimation;
class QPushButton;

class OverlayMask;
class TourCountdown;

// 气泡：圆角底、指向目标的三角箭头、倒计时条都是自绘；里面三层文字
// （标题/正文/秒数）与三个按钮只设 objectName，由 QSS 覆盖。
//
// 从 TourOverlay 里抽出来的（原先气泡的布局、绘制、动画、换挂法全在那边）。
// 它只管"气泡自己"：**什么时候显示、挂在谁身上由 TourOverlay 定**，靶在哪儿
// 问遮罩（OverlayMask），还剩几秒问倒计时（TourCountdown），两个都只读不持有。
class BubbleView : public QWidget {
  Q_OBJECT

 public:
  // overlay 既是父控件（讲解式），也是"提成顶层窗口之后坐标原点在哪儿"的依据
  // （实操式，见 detachToTopLevel）
  BubbleView(const OverlayMask* mask, const TourCountdown* countdown,
             QWidget* overlay);

  void setTitle(const QString& title);
  void setText(const QString& text);
  // 上一步能不能按 / 下一步是不是最后一步（文案换成「完成！」）
  void setNavigation(bool can_prev, bool can_next);
  // 被动模式：三个按钮全隐（没有下一步可点）
  void setButtonsVisible(bool visible);
  // 气泡开关：关掉就只剩高亮框，没有文案
  void setBubbleVisible(bool visible);

  // 按遮罩里的锚点重摆（走动画），viewport 是遮罩自己的尺寸
  void followAnchor(const QSize& viewport);
  // 显隐跟着遮罩走。**两种挂法都躲不掉**（见实现里的注释）
  void syncVisibility();

  // 实操模式：遮罩对鼠标穿透，气泡不能留在被穿透的子树里（那样点不到），
  // 也不能挂回宿主窗口——遮罩是独立顶层窗口，挂在宿主上会被它压在下面
  void detachToTopLevel();
  void attachToOverlay();

 signals:
  void nextClicked();
  void prevClicked();
  void skipClicked();

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  void initUi();
  void initSignals();
  // 秒数与显隐都从倒计时读（它自己不管显示）
  void syncCountdown();

  void paintBubble(QPainter* painter);
  void paintCountdown(QPainter* painter);
  // 气泡面向目标一侧的三角箭头路径（paintBubble 用；无依托步不调用）
  QPainterPath arrowPath() const;
  // 挪到目标左上角（走动画）并记下箭头该朝哪边
  void moveTo(const QPoint& top_left, BubbleSide side);

  const OverlayMask* mask_;
  const TourCountdown* countdown_;
  QWidget* overlay_;

  QLabel* countdown_label_;
  QLabel* title_label_;
  QLabel* text_label_;
  QPushButton* skip_btn_;
  QPushButton* prev_btn_;
  QPushButton* next_btn_;

  QString title_;
  QString text_;
  const int kLayoutMargin = 26;
  int cornerRadius_ = 12;
  // 气泡在目标的哪一侧（箭头画在相反一侧，见 arrowPath）
  BubbleSide side_ = BubbleSide::kLeft;
  // 气泡的显隐开关（见 setBubbleVisible）。**实际显隐还要看遮罩在不在**——
  // 两者合成在 syncVisibility 里
  bool bubble_visible_ = true;
  const int kAnimeDur = 300;
  QPropertyAnimation* anime_;
};

#endif  // FILE_TOUR_BUBBLE_VIEW_H_
