#ifndef STYLE_SHEET_DEMO_ITEM_VIEW_PAGE_H_
#define STYLE_SHEET_DEMO_ITEM_VIEW_PAGE_H_

#include <QWidget>

// 列表 / 树 / 表格演示：QListWidget、QTreeWidget、QTableWidget，
// 用 QSplitter 并排，方便对照 itemview.qss 的 item / header 规则。
class ItemViewPage : public QWidget {
 public:
  explicit ItemViewPage(QWidget* parent = nullptr);

 private:
  void initUi();
};

#endif  // STYLE_SHEET_DEMO_ITEM_VIEW_PAGE_H_