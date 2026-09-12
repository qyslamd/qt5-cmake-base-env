#ifndef FILE_LIBUI_PROGRESS_BARS_WATER_PROGRESS_BAR_H_
#define FILE_LIBUI_PROGRESS_BARS_WATER_PROGRESS_BAR_H_

#include <QColor>

#include "progress_bar_base.h"

// 水波进度条：圆形水球，水位随进度上升，水面用双正弦波模拟。参考
// DWaterProgress 的思路，但不依赖任何外部 SVG 资源。它是本族里唯一常态
// 动画的一个（波浪一直在晃），忙碌态下水位固定在中位、照常起波
class WaterProgress : public ProgressBase {
  Q_OBJECT
  Q_PROPERTY(QColor waterColor READ waterColor WRITE setWaterColor)
  Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor)
  Q_PROPERTY(int borderWidth READ borderWidth WRITE setBorderWidth)
  Q_PROPERTY(qreal waveHeight READ waveHeight WRITE setWaveHeight)

 public:
  explicit WaterProgress(QWidget* parent = nullptr);

  QColor waterColor() const { return water_color_; }
  QColor borderColor() const { return border_color_; }
  int borderWidth() const { return border_width_; }
  qreal waveHeight() const { return wave_height_; }

 public slots:
  void setWaterColor(const QColor& c);
  void setBorderColor(const QColor& c);
  void setBorderWidth(int w);
  void setWaveHeight(qreal h);

 protected:
  // 波纹任何时候都在晃，故恒为动画；基类只在控件可见时才驱动它
  bool isAnimated() const override { return true; }
  void tick() override;

  void paintEvent(QPaintEvent* event) override;

 private:
  QColor water_color_ = QColor(43, 146, 255);
  QColor border_color_ = QColor(43, 146, 255);
  int border_width_ = 3;
  qreal wave_height_ = 6.0;
  // 波浪相位，由 tick 推进、paintEvent 读取
  qreal phase_ = 0.0;
};

#endif  // FILE_LIBUI_PROGRESS_BARS_WATER_PROGRESS_BAR_H_
