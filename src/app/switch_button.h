#ifndef FILE_APP_SWITCH_BUTTON_H_
#define FILE_APP_SWITCH_BUTTON_H_

#include <QAbstractButton>
#include <QBrush>

class QPainter;
class QVariantAnimation;

// 纯滑块开关：胶囊底 + 白色圆形滑块，无文字、无凹槽。
// 自绘而非走 QSS（QAbstractButton 的 QSS 无法表达滑块动画）。
// 配色可通过 setOnBackground / setOffBackground 注入。
class SwitchButton : public QAbstractButton {
  Q_OBJECT

 public:
  explicit SwitchButton(QWidget* parent = nullptr);

  void setOnBackground(const QBrush& brush);
  void setOffBackground(const QBrush& brush);

 protected:
  void paintEvent(QPaintEvent* event) override;
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 private:
  void fillRoundRect(QPainter* painter, const QRectF& rect,
                     const QBrush& brush);
  void startKnobAnimation(bool checked);

  QBrush background_on_ = QColor(0x00, 0x7A, 0xCC);
  QBrush background_off_ = QColor(0xD0, 0xD0, 0xDD);
  QVariantAnimation* knob_animation_ = nullptr;
  qreal knob_position_ = 0.0;  // 0 = off，1 = on，由动画驱动
};

#endif  // FILE_APP_SWITCH_BUTTON_H_
