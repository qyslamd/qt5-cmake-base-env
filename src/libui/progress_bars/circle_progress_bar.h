#ifndef FILE_LIBUI_PROGRESS_BARS_CIRCLE_PROGRESS_BAR_H_
#define FILE_LIBUI_PROGRESS_BARS_CIRCLE_PROGRESS_BAR_H_

#include <QColor>

#include "progress_bar_base.h"

// 圆环进度条：底轨 + 圆头弧线 + 居中百分比文字。忙碌态下一段约 90° 的弧
// 沿圆周扫掠，不画文字
class CircleProgress : public ProgressBase {
  Q_OBJECT
  Q_PROPERTY(int lineWidth READ lineWidth WRITE setLineWidth)
  Q_PROPERTY(QColor color READ color WRITE setColor)
  Q_PROPERTY(
      QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)
  Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)

 public:
  explicit CircleProgress(QWidget* parent = nullptr);

  int lineWidth() const { return line_width_; }
  QColor color() const { return color_; }
  QColor backgroundColor() const { return background_color_; }
  QColor textColor() const { return text_color_; }

 public slots:
  void setLineWidth(int w);
  void setColor(const QColor& c);
  void setBackgroundColor(const QColor& c);
  void setTextColor(const QColor& c);

  // 只有忙碌态需要逐帧重绘，进度弧是静态的
  bool isAnimated() const override { return isBusy(); }
  void tick() override;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  // 忙碌扫掠的相位，0.0 ~ 1.0 循环，一圈对应一次完整扫掠
  qreal phase_ = 0.0;
  int line_width_ = 12;
  QColor color_ = QColor(0, 160, 233);
  QColor background_color_ = QColor(232, 236, 240);
  QColor text_color_ = QColor(0, 160, 233);
  int start_angle_ = 90;  // 12 点方向
  int span_angle_ = 360;  // 整圆
};

#endif  // FILE_LIBUI_PROGRESS_BARS_CIRCLE_PROGRESS_BAR_H_