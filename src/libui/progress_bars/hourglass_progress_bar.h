#ifndef FILE_LIBUI_PROGRESS_BARS_HOURGLASS_PROGRESS_BAR_H_
#define FILE_LIBUI_PROGRESS_BARS_HOURGLASS_PROGRESS_BAR_H_

#include <QColor>

#include "progress_bar_base.h"

// 沙漏进度条：上半部分沙子随进度减少，下半部分随进度增多。进度态纯由
// value 驱动；忙碌态走"漏空 -> 翻转 180 度 -> 反向漏空 -> 翻回"的循环，
// 翻转时沙子随玻璃一起转，"满"的那端翻上来接着漏
class HourglassProgress : public ProgressBase {
  Q_OBJECT
  Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor)
  Q_PROPERTY(QColor sandColor READ sandColor WRITE setSandColor)
  Q_PROPERTY(int frameWidth READ frameWidth WRITE setFrameWidth)

 public:
  explicit HourglassProgress(QWidget* parent = nullptr);

  QColor frameColor() const { return frame_color_; }
  QColor sandColor() const { return sand_color_; }
  int frameWidth() const { return frame_width_; }

 public slots:
  void setFrameColor(const QColor& c);
  void setSandColor(const QColor& c);
  void setFrameWidth(int w);

  // 进度态是静态的，只有忙碌态需要逐帧重绘
  bool isAnimated() const override { return isBusy(); }
  void tick() override;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  // 忙碌循环的相位，0.0 ~ 1.0，一个循环含两次漏空与两次翻转
  qreal phase_ = 0.0;
  QColor frame_color_ = QColor(63, 107, 157);
  QColor sand_color_ = QColor(222, 164, 96);
  int frame_width_ = 6;
};

#endif  // FILE_LIBUI_PROGRESS_BARS_HOURGLASS_PROGRESS_BAR_H_
