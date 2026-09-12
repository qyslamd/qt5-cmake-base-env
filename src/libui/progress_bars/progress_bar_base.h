#ifndef FILE_LIBUI_PROGRESS_BARS_PROGRESS_BAR_BASE_H_
#define FILE_LIBUI_PROGRESS_BARS_PROGRESS_BAR_BASE_H_

#include <QWidget>

class QHideEvent;
class QShowEvent;
class QTimer;

// 所有自绘进度条控件的公共基类。提供 value / minimum / maximum /
// textVisible 四个属性、归一化进度 ratio()（0.0 ~ 1.0），以及动画骨架：
// 定时器由基类持有，只在控件可见且 isAnimated() 为真时运行，子类实现
// tick() 推进自己的相位即可。子类另外只需实现 paintEvent()
//
// 忙碌态（不确定进度）沿用 QProgressBar 的约定：minimum 与 maximum 同为 0
// （即 setRange(0, 0)）即为忙碌。此时 ratio() 没有意义、不画百分比文字，
// 各风格改画自己的不确定动画，而"动"正是走 isAnimated() 这条通道
class ProgressBase : public QWidget {
  Q_OBJECT
  Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
  Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
  Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
  Q_PROPERTY(bool textVisible READ textVisible WRITE setTextVisible)

 public:
  explicit ProgressBase(QWidget* parent = nullptr);
  ~ProgressBase() override;

  int value() const { return value_; }
  int minimum() const { return minimum_; }
  int maximum() const { return maximum_; }
  bool textVisible() const { return text_visible_; }

  // 归一化进度：0.0 ~ 1.0
  qreal ratio() const;

  // 忙碌态：minimum 与 maximum 同为 0。此时进度无意义，改画不确定动画
  bool isBusy() const { return minimum_ == 0 && maximum_ == 0; }

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 public slots:
  virtual void setValue(int value);
  virtual void setMinimum(int min);
  virtual void setMaximum(int max);
  void setRange(int min, int max);
  virtual void setTextVisible(bool visible);

 signals:
  void valueChanged(int value);

 protected:
  // 返回 true 时基类启动逐帧定时器驱动本控件。需要动画的外观通常写成
  // isBusy()：静态时零开销，忙碌时才动起来
  virtual bool isAnimated() const { return false; }

  // 每帧调用一次，子类在此推进自己的相位。相位留在子类，基类不代管
  virtual void tick() {}

  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;

  int value_ = 0;
  int minimum_ = 0;
  int maximum_ = 100;
  bool text_visible_ = true;

 private:
  // 依据可见性与 isAnimated() 启停定时器，不可见时不空转
  void updateTimerState();

  QTimer* timer_ = nullptr;
};

#endif  // FILE_LIBUI_PROGRESS_BARS_PROGRESS_BAR_BASE_H_
