#include "style_sheet_demo_window.h"

#include <QAction>
#include <QDockWidget>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>

#include "buttons_page.h"
#include "container_page.h"
#include "input_page.h"
#include "item_view_page.h"
#include "misc_page.h"
#include "progress_page.h"

StyleSheetDemoWindow::StyleSheetDemoWindow(QWidget* parent)
    : QMainWindow(parent) {
  initUi();
  initSignals();
}

void StyleSheetDemoWindow::initUi() {
  setWindowTitle(QStringLiteral("样式表底座层演示"));
  resize(1120, 780);

  tabs_ = new QTabWidget(this);
  tabs_->addTab(new ButtonsPage(tabs_), QStringLiteral("按钮族"));
  tabs_->addTab(new InputPage(tabs_), QStringLiteral("输入族"));
  tabs_->addTab(new ItemViewPage(tabs_), QStringLiteral("列表/树/表格"));
  tabs_->addTab(new ContainerPage(tabs_), QStringLiteral("容器与滚动"));
  tabs_->addTab(new ProgressPage(tabs_), QStringLiteral("进度与滑块"));
  tabs_->addTab(new MiscPage(tabs_), QStringLiteral("图形与文本"));
  setCentralWidget(tabs_);

  PopulateMenus();
  PopulateToolBar();
  PopulateStatusBar();

  // 弄个停靠窗口（靠右边停着）
  auto* dock = new QDockWidget(QStringLiteral("停靠窗口示例"), this);
  auto* dock_content = new QWidget();
  auto* dock_layout = new QVBoxLayout(dock_content);
  dock_layout->setContentsMargins(8, 8, 8, 8);
  dock_layout->addWidget(
      new QLabel(QStringLiteral("我是一块可以拖拽、可以悬浮的砖。")));
  dock_layout->addStretch();
  dock->setWidget(dock_content);
  addDockWidget(Qt::RightDockWidgetArea, dock);
}

void StyleSheetDemoWindow::initSignals() {
  // 演示页里的控件都是静态的，没有信号需要连接。
}

void StyleSheetDemoWindow::PopulateMenus() {
  auto* file_menu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
  file_menu->addAction(QStringLiteral("新建"));
  file_menu->addAction(QStringLiteral("打开…"));
  file_menu->addSeparator();
  file_menu->addAction(QStringLiteral("退出"));

  auto* edit_menu = menuBar()->addMenu(QStringLiteral("编辑(&E)"));
  edit_menu->addAction(QStringLiteral("撤销"));
  edit_menu->addAction(QStringLiteral("重做"));
  edit_menu->addSeparator();
  edit_menu->addAction(QStringLiteral("剪切"));
  edit_menu->addAction(QStringLiteral("复制"));
  edit_menu->addAction(QStringLiteral("粘贴"));

  auto* view_menu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
  view_menu->addAction(QStringLiteral("放大"));
  view_menu->addAction(QStringLiteral("缩小"));
  view_menu->addAction(QStringLiteral("实际大小"));

  auto* help_menu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
  help_menu->addAction(QStringLiteral("关于"));
}

void StyleSheetDemoWindow::PopulateToolBar() {
  auto* toolbar = addToolBar(QStringLiteral("主工具栏"));

  auto* new_action = toolbar->addAction(QStringLiteral("新建"));
  new_action->setToolTip(QStringLiteral("新建文件（悬停看 QToolTip 样式）"));
  toolbar->addAction(QStringLiteral("打开"));
  toolbar->addAction(QStringLiteral("保存"));
  toolbar->addSeparator();
  toolbar->addAction(QStringLiteral("运行"));
  toolbar->addAction(QStringLiteral("停止"));

  auto* primary = new QToolButton(toolbar);
  primary->setText(QStringLiteral("主操作"));
  primary->setProperty("role", QStringLiteral("primary"));
  primary->setToolTip(QStringLiteral("QToolButton role=\"primary\"（渐变底）"));
  toolbar->addWidget(primary);
}

void StyleSheetDemoWindow::PopulateStatusBar() {
  statusBar()->showMessage(
      QStringLiteral("底座层演示：每个 tab 对应一两个 base/*.qss，"
                     "悬停控件看 QToolTip 样式"));
}