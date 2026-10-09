#ifndef FILE_TOUR_OVERLAY_H_
#define FILE_TOUR_OVERLAY_H_

#include <QList>
#include <QPointer>
#include <QRect>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;
class QPainter;

class BubbleView;
class OverlayMask;
class TourCountdown;
class TourTopic;
class TourStep;

// 演示层的协调者：覆盖整个 viewport 的半透明遮罩 + 目标高亮 + 气泡导航。
//
// 三件事已各自成类：遮罩（OverlayMask）、气泡（BubbleView）、倒计时
// （TourCountdown）。本体只剩"把三者摆到一起"：
//   - 模式：讲解式（点空白退出）/ 实操式（遮罩穿透、气泡提顶层）/ 被动式
//     （只显示、不交互，由外部驱动）
//   - 步骤：updateUi 把主题/步骤翻成标题、锚点、按钮态
//   - 外部驱动的高亮：showHighlight* 指一个控件或一块矩形，说一句话
//   - 宿主窗口的跟随与层级（setHost）
// 自绘层的颜色也不在这一类里，见 tour_colors.h
class TourOverlay : public QWidget {
  Q_OBJECT

  // 本次高亮框了几处（含锚点）。**必须是静态 Q_PROPERTY**：funq 的 properties()
  // 只枚举元对象里声明过的（服务端走 metaObject()->propertyCount()），动态属性
  // 读出来是 KeyError 而不是报错——"框了三个"这件事只有眼睛看得见，这个数字
  // 是它唯一的自动判定出口（《一步多靶设计》§六）
  Q_PROPERTY(int highlightCount READ highlightCount)

 public:
  explicit TourOverlay(QWidget* parent = nullptr);
  // 气泡在实操模式下会提成独立顶层窗口（见 setInteractive 那条 reparent），
  // **脱离了 Qt 的父子所有权**——不在这里显式回收，析构时它就漏了，而且那个
  // 窗口会一直留在屏幕上。仍挂在遮罩下时删它同样安全
  ~TourOverlay() override;

  void updateUi(TourTopic* const topic, TourStep* const step, bool canPrev,
                bool canNext);
  void setIsAutoMode(bool autoMode);
  // 实操模式：遮罩对鼠标穿透（点击落到下层真实控件，页签/滚动条均可操作，
  // 点遮罩空白不再退出），气泡改挂顶层窗口保持可点。
  // 默认关闭 = 讲解式（目标不响应、点空白退出）
  void setInteractive(bool interactive);

  // 被动模式：只显示、不交互，由外部驱动（见 TourBridge）。
  // 三个按钮全隐、倒计时一律停、点遮罩空白不退出
  void setPassiveMode(bool passive);

  // 气泡开关：关掉就只剩高亮框，没有文案
  void setBubbleVisible(bool visible);

  // 跟随一个宿主窗口：把遮罩铺到它上面并置顶。
  //
  // 演示层刻意**不设 Qt 父子**：模态对话框关闭时宿主被销毁，父子关系会把
  // 演示层一起带走；而作为某个窗口的子控件，也永远盖不到同为顶层窗口的模态
  // 框之上。所以改用 QPointer 记住宿主 + 手动 setGeometry/raise 管理层级
  //
  // inflate=true 时遮罩往外扩一圈 kHostMargin。高亮目标**就是宿主窗口本身**
  // 时要传它：那种情况下高亮框会等于整块遮罩，而遮罩是"整块挖掉高亮区"画的，
  // 阴影会被整块掏空，屏幕上只剩气泡。扩出去的部分按屏幕可用区裁掉——宿主
  // 已经占满屏幕时自然就扩不动了，不必单独判全屏
  void setHost(QWidget* host, bool inflate = false);

  // 外部驱动的高亮：不经主题/步骤，直接指一个控件、说一句话。
  // 桥接层配合 setPassiveMode(true) 使用。
  //
  // extras 是**锚点之外还要一起框住**的控件（《一步多靶设计》）：给了就一起挖
  // 孔，气泡仍挂在锚点旁。**按参数无条件赋值**——没给就是空表，上一步的框要
  // 跟着消失
  void showHighlight(QWidget* target, const QString& title, const QString& text,
                     const QList<QWidget*>& extras = QList<QWidget*>());
  // 同上，但指一块**全局坐标的矩形**而不是控件。
  //
  // 页签（`QTabBar` 的某一个 tab）不是 QWidget、只是画出来的，按 path 找不到
  // 它；桥接那边用 `tabRect()` 把目标页签的矩形算出来，走这一条送进来。
  // host 是这块矩形所在的窗口——遮罩要铺到它上面
  void showHighlightRect(QWidget* host, const QRect& global_rect,
                         const QString& title, const QString& text,
                         const QList<QRect>& extras = QList<QRect>());
  // 视觉倒计时：气泡上走一条进度条与秒数，到点自己停。
  //
  // **不推进任何东西**（不 emit nextClicked）：外部驱动那套里下一步做什么由
  // 脚本决定，这里只让"马上要切页了"这件事看得见
  void showCountdown(int seconds);
  void clearHighlight();

  // 最近一次高亮的矩形（遮罩坐标系）。桥接层回报给脚本侧核对：高亮和遮罩
  // 到底算在了哪儿，是"看不见"这类问题唯一能拿到的数字
  QRect highlightRect() const;

  // 本次高亮**一共框了几处**（含锚点）。给 funq 观测用：单靶恒为 1，
  // 多靶是几就是几——"框了三个"这件事只有眼睛看得见，这个数字是它唯一的出口。
  // 桥接层也读它填返回值的 count
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

  // 摆气泡：算法在 tour_bubble_placer（纯几何、可单测），坐标原点在气泡自己
  // 那儿换算（它才知道自己挂在遮罩上还是挂了顶层窗口）
  void changeBubblePosition();

  // 两个 showHighlight* 的收尾：摆气泡、刷出来、显示并置顶。
  // 矩形已经算好交给遮罩了
  void finishShowHighlight(const QString& title, const QString& text);

  // 遮罩要不要吞鼠标，由**两个标志共同决定**（被动模式 / 实操模式），只在
  // 这一个出口写 WA_TransparentForMouseEvents。
  //
  // 原先 setInteractive 与 setPassiveMode 各写一句同一个属性，而控制器里是
  // 连着调的（applyTopicOptions：先 setInteractive(topic->interactive())，紧跟
  // setPassiveMode(false)），后者无条件把前者刚设的 true 盖回 false——实操
  // 主题的"遮罩穿透、边看边动手"因此从来没生效过，点被高亮的控件点不动、
  // 点遮罩空白反而退出。同一份状态两个写者，是模式状态那类缺陷的同一形态
  void updateMouseTransparency();

  OverlayMask* mask_;
  TourCountdown* countdown_;
  BubbleView* bubble_;

  // 遮罩跟随的宿主窗口。不设 Qt 父子，所以用 QPointer 防悬空
  QPointer<QWidget> host_;
  // 被动模式（外部驱动）见对应 setter
  bool passive_mode_ = false;
  // 实操模式（见 setInteractive）：遮罩子树穿透，气泡挂在 viewport 上
  bool interactive_ = false;
  // 遮罩比宿主窗口外扩的像素数，见 setHost
  const int kHostMargin = 24;
};

#endif  // FILE_TOUR_OVERLAY_H_
