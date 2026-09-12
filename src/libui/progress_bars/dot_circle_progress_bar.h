#ifndef FILE_LIBUI_PROGRESS_BARS_DOT_CIRCLE_PROGRESS_BAR_H_
#define FILE_LIBUI_PROGRESS_BARS_DOT_CIRCLE_PROGRESS_BAR_H_

#include <QColor>

#include "progress_bar_base.h"

// 圆点拖尾进度条：一个亮点沿圆周运动，身后带渐隐拖尾。原实现用
// QVariantAnimation 自走动画，收编时改为不自走——亮点位置只读 ratio()
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

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  QColor color_ = QColor(64, 158, 255);
  QColor track_color_ = QColor(230, 234, 240);
  int dot_radius_ = 9;
  int tail_count_ = 10;
  int track_width_ = 4;
  qreal tail_step_ = 0.015;  // 拖尾点之间的百分比间距
};

#endif  // FILE_LIBUI_PROGRESS_BARS_DOT_CIRCLE_PROGRESS_BAR_H_
