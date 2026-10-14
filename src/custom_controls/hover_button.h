#ifndef FILE_APP_WIDGETS_HOVER_BUTTON_H_
#define FILE_APP_WIDGETS_HOVER_BUTTON_H_

#include <QAbstractButton>
#include <QColor>
#include <QIcon>

class QPropertyAnimation;

// 纯自绘按钮：hover 时原文字右滑淡出、accent 覆盖层淡入、
// 白色文字配手绘箭头，220ms OutCubic。
// 两种变体：
//   Primary   —— 实心底 + 文字（主操作，无前导图标）
//   Secondary —— 浅色底 + 细边框（次操作，未 hover 时可带前导图标）
// 自绘而非走 QSS：文字滑动 + 箭头是 QSS 表达不了的。
// 版式（pointSize 11.5 / DemiBold）写死在 paintEvent 里，只暴露配色。
//
// 文字内容不走自己的成员：构造函数里调 QAbstractButton::setText 存进基类，
// paintEvent 里读继承来的 text()。所以通过 QAbstractButton* 改名也能同步。
//
// 来源：从 examples/new_login_dialog 抽出的通用控件
class HoverButton : public QAbstractButton {
  Q_OBJECT
  Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)

 public:
  enum Variant { Primary, Secondary };

  HoverButton(const QString& text, Variant v, QWidget* parent = nullptr);

  // 前导图标只在 Secondary 变体、且未 hover 时绘制
  void setLeadingIcon(const QIcon& icon);

  // hover 覆盖层颜色，默认 #5b21b6
  void setAccentColor(const QColor& c);

  // ---- 配色注入（默认即登录示例那套）----
  // Primary 的文字色只在未 hover 时用于淡出；hover 态文字 + 箭头恒为白色
  // （假定 accent 是饱和深色）。
  void setPrimaryBackground(const QColor& c);
  void setPrimaryTextColor(const QColor& c);
  void setSecondaryBackground(const QColor& c);
  void setSecondaryBorder(const QColor& c);
  void setSecondaryTextColor(const QColor& c);

  qreal hoverProgress() const { return hp_; }
  void setHoverProgress(qreal p);

  QSize sizeHint() const override { return QSize(300, 50); }

 protected:
  void paintEvent(QPaintEvent*) override;
  void enterEvent(QEvent*) override;
  void leaveEvent(QEvent*) override;

 private:
  QIcon icon_;
  Variant variant_;
  QColor accent_{"#5b21b6"};
  QColor primary_bg_{"#1a1a2e"};
  QColor primary_text_{Qt::white};
  QColor secondary_bg_{"#f5f5f5"};
  QColor secondary_border_{"#e0e0e0"};
  QColor secondary_text_{"#333333"};
  qreal hp_ = 0.0;
  QPropertyAnimation* anim_ = nullptr;
};

#endif  // FILE_APP_WIDGETS_HOVER_BUTTON_H_