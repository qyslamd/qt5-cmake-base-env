#ifndef FILE_LIBUI_PROGRESS_BARS_DOT_CIRCLE_PROGRESS_BAR_H_
#define FILE_LIBUI_PROGRESS_BARS_DOT_CIRCLE_PROGRESS_BAR_H_

#include <QColor>

#include "progress_bar_base.h"

// 圆点拖尾进度条：一个亮点沿圆周运动，身后带渐隐拖尾。进度态下亮点位置
// 只读 ratio()，不自走；忙碌态下亮点以相位为位置持续绕圈，拖尾跨过起点
// 接到上一圈，回到它收编前的原始形态
class DotCircleProgress : public ProgressBase {
  Q_OBJECT
  Q_PROPERTY(QColor color READ color WRITE setColor)
  Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor)
  Q_PROPERTY(int dotRadius READ dotRadius WRITE setDotRadius)
  Q_PROPERTY(int tailCount READ tailCount WRITE setTailCount)

 public:
  explicit DotCircleProgress(QWidget* parent = nullptr);

  QColor color() const { return color_; }
  QColor trackColor() const { return track_color_; }
  int dotRadius() const { return dot_radius_; }
  int tailCount() const { return tail_count_; }

 public slots:
  void setColor(const QColor& c);
  void setTrackColor(const QColor& c);
  void setDotRadius(int r);
  void setTailCount(int n);

  // 进度态是静态的，只有忙碌态需要逐帧重绘
  bool isAnimated() const override { return isBusy(); }
  void tick() override;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  // 忙碌绕圈的相位，0.0 ~ 1.0 循环
  qreal phase_ = 0.0;
  QColor color_ = QColor(64, 158, 255);
  QColor track_color_ = QColor(230, 234, 240);
  int dot_radius_ = 9;
  int tail_count_ = 10;
  int track_width_ = 4;
  qreal tail_step_ = 0.015;  // 拖尾点之间的百分比间距
};

#endif  // FILE_LIBUI_PROGRESS_BARS_DOT_CIRCLE_PROGRESS_BAR_H_
