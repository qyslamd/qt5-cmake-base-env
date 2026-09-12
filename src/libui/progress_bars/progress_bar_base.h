#ifndef FILE_LIBUI_PROGRESS_BARS_PROGRESS_BAR_BASE_H_
#define FILE_LIBUI_PROGRESS_BARS_PROGRESS_BAR_BASE_H_

#include <QWidget>

// 所有自绘进度条控件的公共基类。提供 value / minimum / maximum /
// textVisible 四个属性，以及归一化进度 ratio()（0.0 ~ 1.0），
// 子类只需实现 paintEvent()
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

 public slots:
  virtual void setValue(int value);
  virtual void setMinimum(int min);
  virtual void setMaximum(int max);
  void setRange(int min, int max);
  virtual void setTextVisible(bool visible);

 signals:
  void valueChanged(int value);

 protected:
  int value_ = 0;
  int minimum_ = 0;
  int maximum_ = 100;
  bool text_visible_ = true;
};

#endif  // FILE_LIBUI_PROGRESS_BARS_PROGRESS_BAR_BASE_H_