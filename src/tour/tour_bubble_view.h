#ifndef FILE_TOUR_BUBBLE_VIEW_H_
#define FILE_TOUR_BUBBLE_VIEW_H_

#include <QWidget>

#include "tour_bubble_placer.h"

class QLabel;
class QPainter;
class QPainterPath;
class QPropertyAnimation;
class QPushButton;

class TourCountdown;

// 气泡：圆角底、指向目标的三角箭头、倒计时条都是自绘；三层文字与三个按钮
// 只设 objectName，由 QSS 覆盖。
//
// 只管"气泡自己"：什么时候显示、挂在谁身上由 TourOverlay 定；靶按次给
// （见 followAnchor），还剩几秒问 TourCountdown（只读不持有）
class TourBubbleView : public QWidget {
  Q_OBJECT

 public:
  // overlay 既是父控件，也是提成顶层窗口后换算坐标原点的依据（见
  // detachToTopLevel）
  TourBubbleView(const TourCountdown* countdown, QWidget* overlay);

  void setTitle(const QString& title);
  void setText(const QString& text);
  // 上一步能不能按 / 下一步是不是最后一步（文案换成「完成！」）
  void setNavigation(bool can_prev, bool can_next);
  // 被动模式：三个按钮全隐（没有下一步可点）
  void setButtonsVisible(bool visible);
  // 气泡开关：关掉就只剩高亮框，没有文案
  void setBubbleVisible(bool visible);

  // 按锚点重摆（走动画），viewport 是遮罩自己的尺寸。
  //
  // 锚点**按次给、不持有**：摆位与"箭头画不画"由同一个矩形在这一个函数里定，
  // 不会两处各读一份状态。空矩形 = 纯讲解步：居中、不画箭头
  void followAnchor(const QRect& anchor, const QSize& viewport);
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
  // 布局边距（箭头与倒计时条都从它推）、圆角、位移动画时长
  static constexpr int kLayoutMargin = 26;
  static constexpr int kCornerRadius = 12;
  static constexpr int kAnimeDur = 300;

  void initUi();
  void initSignals();
  // 倒计时的显示**只有这一处决定**：秒数、显隐、进度条画不画都在这儿落地
  void updateCountdownDisplay();

  void paintBubble(QPainter* painter);
  void paintCountdown(QPainter* painter);
  // 气泡面向目标一侧的三角箭头路径（paintBubble 用；无靶步不调用）
  QPainterPath arrowPath() const;
  // 挪到目标左上角（走动画）并记下箭头该朝哪边
  void moveTo(const QPoint& top_left, BubbleSide side);

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
  // 本步有没有靶（followAnchor 按入参落地）：有才画三角箭头
  bool has_anchor_ = false;
  // 倒计时此刻的显示状态（只有 updateCountdownDisplay 写它）。不是第二份真相：
  // 它只在倒计时的 changed 信号里刷新
  bool countdown_shown_ = false;
  // 气泡在目标的哪一侧（箭头画在相反一侧，见 arrowPath）
  BubbleSide side_ = BubbleSide::kLeft;
  // 气泡开关（见 setBubbleVisible）；实际显隐还要看遮罩在不在，两者合成在
  // syncVisibility 里
  bool bubble_visible_ = true;
  QPropertyAnimation* anime_;
};

#endif  // FILE_TOUR_BUBBLE_VIEW_H_
