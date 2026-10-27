#ifndef STYLE_SHEET_DEMO_CONTAINER_PAGE_H_
#define STYLE_SHEET_DEMO_CONTAINER_PAGE_H_

#include <QWidget>

// 容器与滚动演示：QGroupBox、QSplitter、QScrollArea（带边框与圆角）。
class ContainerPage : public QWidget {
 public:
  explicit ContainerPage(QWidget* parent = nullptr);

 private:
  void initUi();
};

#endif  // STYLE_SHEET_DEMO_CONTAINER_PAGE_H_