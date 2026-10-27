#ifndef STYLE_SHEET_DEMO_BUTTONS_PAGE_H_
#define STYLE_SHEET_DEMO_BUTTONS_PAGE_H_

#include <QWidget>

// 按钮族演示：QPushButton / QToolButton 的 role 变体、QCheckBox /
// QRadioButton、QDialogButtonBox。
class ButtonsPage : public QWidget {
 public:
  explicit ButtonsPage(QWidget* parent = nullptr);

 private:
  void initUi();
};

#endif  // STYLE_SHEET_DEMO_BUTTONS_PAGE_H_