#ifndef FILE_CUSTOM_CONTROLS_PASSWORD_EYE_BUTTON_H_
#define FILE_CUSTOM_CONTROLS_PASSWORD_EYE_BUTTON_H_

#include <QAbstractButton>
#include <QColor>

// 密码可见性开关：可勾选按钮，**checked = 密码可见**。
// 勾选态画睁开的眼睛，未勾选画一道斜杠。
//
// 自绘而非图标：颜色要能被样式表驱动，用 qproperty-eyeColor /
// qproperty-hoverEyeColor 配即可——图标方案的颜色烧在 SVG 里，换不了。
// hover 由控件自己认（enter/leave 换色重绘），不靠样式表的 :hover
// 伪状态去重刷属性：qproperty- 只在 polish 时写一次，实测悬停不变色。
class PasswordEyeButton : public QAbstractButton {
  Q_OBJECT
  Q_PROPERTY(QColor eyeColor READ eyeColor WRITE setEyeColor)
  Q_PROPERTY(QColor hoverEyeColor READ hoverEyeColor WRITE setHoverEyeColor)

 public:
  explicit PasswordEyeButton(QWidget* parent = nullptr);

  QColor eyeColor() const { return eye_color_; }
  void setEyeColor(const QColor& c);

  QColor hoverEyeColor() const { return hover_eye_color_; }
  void setHoverEyeColor(const QColor& c);

  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void enterEvent(QEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  QColor eye_color_{QStringLiteral("#666666")};
  QColor hover_eye_color_{QStringLiteral("#333333")};
  bool hovered_ = false;
};

#endif  // FILE_CUSTOM_CONTROLS_PASSWORD_EYE_BUTTON_H_
