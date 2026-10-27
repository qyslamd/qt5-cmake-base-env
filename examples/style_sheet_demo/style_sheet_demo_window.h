#ifndef STYLE_SHEET_DEMO_STYLE_SHEET_DEMO_WINDOW_H_
#define STYLE_SHEET_DEMO_STYLE_SHEET_DEMO_WINDOW_H_

#include <QMainWindow>

class QTabWidget;

// 底座样式表演示主窗口：每个 tab 对应一两个 base/*.qss，菜单 / 工具栏 /
// 状态栏单独装配，方便对照 menu.qss。
class StyleSheetDemoWindow : public QMainWindow {
 public:
  explicit StyleSheetDemoWindow(QWidget* parent = nullptr);

 private:
  void initUi();
  void initSignals();

  void PopulateMenus();
  void PopulateToolBar();
  void PopulateStatusBar();

  QTabWidget* tabs_ = nullptr;
};

#endif  // STYLE_SHEET_DEMO_STYLE_SHEET_DEMO_WINDOW_H_