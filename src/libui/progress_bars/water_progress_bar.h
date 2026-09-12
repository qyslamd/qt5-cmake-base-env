#ifndef FILE_LIBUI_PROGRESS_BARS_WATER_PROGRESS_BAR_H_
#define FILE_LIBUI_PROGRESS_BARS_WATER_PROGRESS_BAR_H_

#include <QColor>

#include "progress_bar_base.h"

class QTimer;

// 水波进度条：圆形水球，水位随进度上升，水面用双正弦波模拟。参考
// DWaterProgress 的思路，但不依赖任何外部 SVG 资源。波浪相位由内部
// 定时器推进，是本族里唯一自带动画的一个
class WaterProgress : public ProgressBase {
  Q_OBJECT
  Q_PROPERTY(QColor waterColor READ waterColor WRITE setWaterColor)
  Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor)
  Q_PROPERTY(int borderWidth READ borderWidth WRITE setBorderWidth)
  Q_PROPERTY(qreal waveHeight READ waveHeight WRITE setWaveHeight)

 public:
  explicit WaterProgress(QWidget* parent = nullptr);

  QColor waterColor() const { return waterColor_; }
  QColor borderColor() const { return borderColor_; }
  int borderWidth() const { return borderWidth_; }
  qreal waveHeight() const { return waveHeight_; }

 public slots:
  void setWaterColor(const QColor& c);
  void setBorderColor(const QColor& c);
  void setBorderWidth(int w);
  void setWaveHeight(qreal h);
  void startWave();
  void stopWave();

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  QColor waterColor_ = QColor(43, 146, 255);
  QColor borderColor_ = QColor(43, 146, 255);
  int borderWidth_ = 3;
  qreal waveHeight_ = 6.0;
  qreal phase_ = 0.0;
  QTimer* timer_ = nullptr;
};

#endif  // FILE_LIBUI_PROGRESS_BARS_WATER_PROGRESS_BAR_H_
