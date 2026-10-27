#ifndef STYLE_SHEET_DEMO_INPUT_PAGE_H_
#define STYLE_SHEET_DEMO_INPUT_PAGE_H_

#include <QWidget>

// 输入族演示：QLineEdit 的各变体、QTextEdit / QPlainTextEdit、
// QAbstractSpinBox 族、QComboBox。
class InputPage : public QWidget {
 public:
  explicit InputPage(QWidget* parent = nullptr);

 private:
  void initUi();
};

#endif  // STYLE_SHEET_DEMO_INPUT_PAGE_H_