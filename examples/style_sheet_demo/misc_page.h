#ifndef STYLE_SHEET_DEMO_MISC_PAGE_H_
#define STYLE_SHEET_DEMO_MISC_PAGE_H_

#include <QWidget>

// 图形与文本演示：QGraphicsView（透明底 + 圆角边框）、QLabel 的
// 默认 / role="error" / role="hint" / disabled 四种状态。
class MiscPage : public QWidget {
 public:
  explicit MiscPage(QWidget* parent = nullptr);

 private:
  void initUi();
};

#endif  // STYLE_SHEET_DEMO_MISC_PAGE_H_