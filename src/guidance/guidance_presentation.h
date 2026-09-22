#ifndef FILE_GUIDANCE_PRESENTATION_H_
#define FILE_GUIDANCE_PRESENTATION_H_

#include <QColor>
#include <QPixmap>
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
// 自绘层（遮罩/气泡底/箭头/高亮框）颜色读 ThemeManager token，明暗主题自动适配；
// 气泡内部子控件（title/text/countdown/skip/prev/next）只设 objectName，由 QSS 覆盖。
class GuidancePresentation : public QWidget {
  Q_OBJECT
 public:
  explicit GuidancePresentation(QWidget* parent = nullptr);

  void updateUi(GuidanceFlow* const flow,
                GuidanceStep* const step,
                bool canPrev,
                bool canNext);
  void setIsAutoMode(bool autoMode);
  // 实操模式：遮罩子树对鼠标穿透（点击落到下层真实控件，页签/滚动条
  // 均可操作，点遮罩空白不再退出），气泡 reparent 到 viewport 保持可点。
  // 默认关闭 = 讲解式（目标不响应、点空白退出），原行为不变
  void setInteractive(bool interactive);

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

  // 自绘层颜色：读 ThemeManager token（明暗主题自动适配），禁止硬编码色值。
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

  // 搞动画的
  bool use_anime_ = true;
  const int kAnimeDur = 300;
  QVariantAnimation* anime_highlight_;
  QPropertyAnimation* anime_bubble_;
  QRect anime_highlight_rect_;

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
};

 
#endif  // FILE_GUIDANCE_PRESENTATION_H_
