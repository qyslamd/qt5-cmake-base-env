#ifndef FILE_GUIDANCE_PRESENTATION_H_
#define FILE_GUIDANCE_PRESENTATION_H_

#include <QColor>
#include <QPixmap>
#include <QPointer>
#include <QWidget>

class QEvent;
class QLabel;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QPropertyAnimation;
class QPushButton;
class QTimer;
class QVariantAnimation;

 

class GuidanceFlow;
class GuidanceStep;

// 演示层：覆盖整个 viewport 的半透明遮罩 + 目标高亮 + 气泡导航。
// 自绘层（遮罩/气泡底/箭头/高亮框）的颜色由 maskColor/bubbleColor/accentColor
// 三个函数收口——目前是写死的常量，要做主题适配改这三处即可；
// 气泡内部子控件（title/text/countdown/skip/prev/next）只设 objectName，由 QSS 覆盖。
class GuidancePresentation : public QWidget {
  Q_OBJECT
 public:
  explicit GuidancePresentation(QWidget* parent = nullptr);
  // 实操模式下气泡被提成了独立顶层窗口（见 setInteractive 里的
  // setParent(nullptr)），**脱离了 Qt 的父子所有权**——不在这里显式回收，
  // 析构时它就漏了，而且那个窗口会一直留在屏幕上。仍挂在遮罩下时删它同样安全
  ~GuidancePresentation() override;

  void updateUi(GuidanceFlow* const flow,
                GuidanceStep* const step,
                bool canPrev,
                bool canNext);
  void setIsAutoMode(bool autoMode);
  // 实操模式：遮罩对鼠标穿透（点击落到下层真实控件，页签/滚动条均可操作，
  // 点遮罩空白不再退出），气泡改挂顶层窗口保持可点。
  // 默认关闭 = 讲解式（目标不响应、点空白退出）
  void setInteractive(bool interactive);

  // 被动模式：只显示、不交互，由外部驱动（见 GuidanceBridge）。
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
  void SetHost(QWidget* host, bool inflate = false);

  // 外部驱动的高亮：不经主题/步骤，直接指一个控件、说一句话。
  // 桥接层配合 setPassiveMode(true) 使用
  void showHighlight(QWidget* target, const QString& title,
                     const QString& text);
  void clearHighlight();

  // 最近一次高亮的矩形（遮罩坐标系）。桥接层回报给脚本侧核对：高亮和遮罩
  // 到底算在了哪儿，是"看不见"这类问题唯一能拿到的数字
  QRect highlightRect() const { return highlightRect_; }

 protected:
  void setTitle(const QString& title);
  void setText(const QString& text);
  // 气泡可能 reparent 在 viewport 上（实操模式）：显隐随之同步，
  // 否则 stop() 只藏了遮罩、气泡残留，下一轮 show 后气泡先于遮罩出现
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;

 signals:
  void nextClicked();
  void prevClicked();
  void skipClicked();

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

 private:
  void initUi();
  void initOthers();
  void initSignals();

  // 在 bubble 上绘制倒计时进度
  void drawCountDownOnBubble(QPainter* painter);
  void drawBubble(QPainter* painter);
  // 气泡面向目标一侧的三角箭头路径（drawBubble 用；无依托步不调用）
  QPainterPath ArrowPath() const;
  void drawHighlight(QPainter* painter);

  void changeBubblePosition();
  struct DirectionInfo {
    bool canPlace = false;
    int spaceWidth = 0;
    int spaceHeight = 0;
  };
  enum Position { Auto, Left, Right, Top, Bottom } position_ = Left;
  DirectionInfo evaluateDirectionSpace(Position pos);

  // 自绘层颜色：集中在这三个函数里，便于统一替换成主题色
  QColor maskColor() const;
  QColor bubbleColor() const;
  QColor accentColor() const;

 private:
  // bubble members
  QWidget* bubble_widget_;
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

  // overlay members
  QRect highlightRect_;
  QPixmap pixmap_;
  int margin_ = 10;
  int borderWidth_ = 3;
  // 遮罩比宿主窗口外扩的像素数，见 SetHost
  const int kHostMargin = 24;

  // 搞动画的
  bool use_anime_ = true;
  const int kAnimeDur = 300;
  QVariantAnimation* anime_highlight_;
  QPropertyAnimation* anime_bubble_;
  QRect anime_highlight_rect_;

  // 控件 → 遮罩坐标系的高亮矩形。updateUi 的 QWidget* 分支与外部驱动的
  // 高亮（showHighlight）共用同一套算法，避免两处算出不同的框
  void ApplyWidgetHighlight(QWidget* target);

  // 遮罩要不要吞鼠标，由**两个标志共同决定**（被动模式 / 实操模式），只在
  // 这一个出口写 WA_TransparentForMouseEvents。
  //
  // 原先 setInteractive 与 setPassiveMode 各写一句同一个属性，而控制器里是
  // 连着调的（ApplyFlowOptions：先 setInteractive(flow->interactive())，紧跟
  // setPassiveMode(false)），后者无条件把前者刚设的 true 盖回 false——实操
  // 主题的"遮罩穿透、边看边动手"因此从来没生效过，点被高亮的控件点不动、
  // 点遮罩空白反而退出。同一份状态两个写者，是模式状态那类缺陷的同一形态
  void UpdateMouseTransparency();

  // 气泡 move() 的坐标原点：气泡挂在遮罩上时是遮罩局部坐标，改挂顶层窗口
  // 之后是屏幕坐标，两处定位都要带上这个偏移
  QPoint BubbleOrigin() const;

  // 自动执行用的
  bool auto_mode_ = false;
  // 实操模式（见 setInteractive）：遮罩子树穿透，气泡挂在 viewport 上
  bool interactive_ = false;
  QTimer* count_down_timer_;
  // 倒计时秒数、tick 间隔、以及由此推出的总 tick 数。三者同源，改时长只需改
  // kCountDownSeconds
  const int kCountDownSeconds = 5;
  const int kCountDownTickMs = 30;
  const int kCountDownMaxTicks = kCountDownSeconds * 1000 / kCountDownTickMs;
  // 本次倒计时"已过去"的比例：0.0 = 刚开始（条子满、秒数最大）。起表时必须
  // 复位成 0.0——它是已过去不是剩余，给 1.0 会让进度条第一帧是空的
  qreal time_percent_ = 0.0;
  // 本次自动倒计时已走的 tick 数；每次开始/停止都复位，避免续接上次残留值。
  int count_down_tick_ = 0;

  // 遮罩跟随的宿主窗口。不设 Qt 父子，所以用 QPointer 防悬空
  QPointer<QWidget> host_;
  // 被动模式（外部驱动）与气泡开关，见对应 setter
  bool passive_mode_ = false;
  bool bubble_visible_ = true;
};

 
#endif  // FILE_GUIDANCE_PRESENTATION_H_
