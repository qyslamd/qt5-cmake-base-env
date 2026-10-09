#ifndef FILE_TOUR_OVERLAY_H_
#define FILE_TOUR_OVERLAY_H_

#include <QList>
#include <QPointer>
#include <QRect>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;
class QPainter;

class TourBubbleView;
class TourOverlayMask;
class TourCountdown;
class TourTopic;
class TourStep;

// 演示层的协调者：遮罩 / 气泡 / 倒计时各是一个类（见那三个头文件），本体只
// 负责把三者摆到一起——三种模式（讲解 / 实操 / 被动）、按步骤刷内容、外部
// 驱动的高亮、宿主窗口的跟随与层级。自绘颜色见 tour_colors.h
class TourOverlay : public QWidget {
  Q_OBJECT

  // 本次高亮框了几处（含锚点）。**必须是静态 Q_PROPERTY**：funq 只枚举元对象
  // 里声明过的属性，"框了三个"这件事只有眼睛看得见，这个数字是它唯一的出口
  Q_PROPERTY(int highlightCount READ highlightCount)

 public:
  explicit TourOverlay(QWidget* parent = nullptr);
  // 气泡在实操模式下会提成独立顶层窗口、脱离 Qt 的父子所有权，得在这里显式
  // 回收；仍挂在遮罩下时删它同样安全
  ~TourOverlay() override;

  void updateUi(TourTopic* const topic, TourStep* const step, bool canPrev,
                bool canNext);
  // 自动挡开关。纯转发给倒计时，留着是为了不让控制器知道那个类
  void setIsAutoMode(bool autoMode);
  // 实操模式：遮罩对鼠标穿透（点击落到下层真实控件），气泡改挂顶层窗口保持
  // 可点。默认关闭 = 讲解式（目标不响应、点空白退出）
  void setInteractive(bool interactive);

  // 被动模式：只显示、不交互，由外部驱动（见 TourBridge）。
  // 三个按钮全隐、倒计时一律停、点遮罩空白不退出
  void setPassiveMode(bool passive);

  // 气泡开关：关掉就只剩高亮框，没有文案
  void setBubbleVisible(bool visible);

  // 跟随宿主窗口：把遮罩铺到它上面并置顶。**不设 Qt 父子**——模态框关闭时
  // 宿主被销毁，父子关系会把演示层一起带走；作为子控件又盖不到顶层模态框
  // 之上。所以用 QPointer 记宿主 + 手动 setGeometry/raise。
  //
  // inflate=true 往外扩一圈 kHostMargin：高亮目标**就是宿主**时必须传，否则
  // 整块遮罩被挖空、只剩气泡。扩出去的部分按屏幕可用区裁掉
  void setHost(QWidget* host, bool inflate = false);

  // 外部驱动的高亮：不经主题/步骤，直接指一个控件、说一句话（配合
  // setPassiveMode(true)）。extras 是锚点之外还要一起框住的控件，**按参数
  // 无条件赋值**——不传就是空表，否则上一步的框会留在屏幕上
  void showHighlight(QWidget* target, const QString& title, const QString& text,
                     const QList<QWidget*>& extras = QList<QWidget*>());
  // 同上，但指一块**全局坐标的矩形**而不是控件：页签不是 QWidget、只是画出来
  // 的，桥接用 tabRect() 算好送进来。host 是矩形所在的窗口
  void showHighlightRect(QWidget* host, const QRect& global_rect,
                         const QString& title, const QString& text,
                         const QList<QRect>& extras = QList<QRect>());
  // 视觉倒计时：气泡上走一条进度条与秒数，到点自己停。
  // **不推进任何东西**——下一步做什么由脚本决定，这里只让"马上要切页"看得见
  void showCountdown(int seconds);
  void clearHighlight();

  // 最近一次高亮的矩形（遮罩坐标系）：高亮算在哪儿，出问题时只能靠这个数字
  QRect highlightRect() const;

  // 本次高亮一共框了几处（含锚点）。桥接层也读它填返回值的 count
  int highlightCount() const;

 protected:
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;

 signals:
  void nextClicked();
  void prevClicked();
  void skipClicked();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

 private:
  void initUi();
  void initSignals();

  // 摆气泡。算法在 tour_bubble_placer，坐标原点由气泡自己换算
  void changeBubblePosition();

  // 两个 showHighlight* 的收尾：摆气泡、刷出来、显示并置顶
  void finishShowHighlight(const QString& title, const QString& text);

  // 遮罩吞不吞鼠标由**被动 / 实操两个标志共同决定**，只在这一个出口写
  // WA_TransparentForMouseEvents——两个 setter 各写一句时，后设的会把先设的抹掉
  void updateMouseTransparency();

  TourOverlayMask* mask_;
  TourCountdown* countdown_;
  TourBubbleView* bubble_;

  // 遮罩跟随的宿主窗口。不设 Qt 父子，用 QPointer 防悬空
  QPointer<QWidget> host_;
  // 被动模式（外部驱动）见对应 setter
  bool passive_mode_ = false;
  // 实操模式（见 setInteractive）：遮罩子树穿透，气泡挂在 viewport 上
  bool interactive_ = false;
  // 遮罩比宿主窗口外扩的像素数，见 setHost
  static constexpr int kHostMargin = 24;
};

#endif  // FILE_TOUR_OVERLAY_H_
