#include "mainwindow.h"

#include <QAction>
#include <QCoreApplication>
#include <QKeySequence>
#include <QLabel>
#include <QVBoxLayout>

// clang-format off
// 功能区与普通菜单栏的构建方式不同，各自只包含自己那套头文件
#ifdef APP_ENABLE_RIBBON
#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonPanel.h>
#else
#include <QMenu>
#include <QMenuBar>
#endif
// clang-format on

#include "dialogs/about_dialog.h"
#include "dialogs/settings_dialog.h"
#include "log_window.h"

MainWindow::MainWindow(QWidget* parent) : MainWindowBase(parent) {
  initUi();
  initSignals();
}

MainWindow::~MainWindow() {}

void MainWindow::initUi() {
  setWindowTitle(QCoreApplication::applicationName());
  setMinimumSize(800, 600);

  createActions();
#ifdef APP_ENABLE_RIBBON
  initRibbon();
#else
  initMenuBar();
#endif
  initCentralWidget();
}

// 动作：objectName 供功能引导与 funq 按名字定位。
// 不给名字的话 funq 会拿「类名-序号」当路径（QAction-2 之类），那种合成名字
// 既不稳定、引导侧也解析不了（它按 objectName 找）
void MainWindow::createActions() {
  quit_action_ = new QAction(QStringLiteral("退出(&Q)"), this);
  quit_action_->setObjectName(QStringLiteral("actionQuit"));
  quit_action_->setShortcut(QKeySequence::Quit);
  connect(quit_action_, &QAction::triggered, this, &QWidget::close);

  log_action_ = new QAction(QStringLiteral("日志窗口(&L)"), this);
  log_action_->setObjectName(QStringLiteral("actionLogWindow"));
  connect(log_action_, &QAction::triggered, this, &MainWindow::onShowLogWindow);

  settings_action_ = new QAction(QStringLiteral("设置(&S)"), this);
  settings_action_->setObjectName(QStringLiteral("actionSettings"));
  connect(settings_action_, &QAction::triggered, this,
          &MainWindow::onShowSettings);

  about_action_ = new QAction(QStringLiteral("关于(&A)"), this);
  about_action_->setObjectName(QStringLiteral("actionAbout"));
  connect(about_action_, &QAction::triggered, this, &MainWindow::onShowAbout);
}

#ifdef APP_ENABLE_RIBBON
// 功能区：一个「主页」分类下挂三个面板。
// 面板也要 objectName——菜单项本身是 QAction（不是 QWidget），引导的高亮与
// funq 的定位都落在控件上，指不到动作，只能指它所在的面板
void MainWindow::initRibbon() {
  SARibbonBar* ribbon = ribbonBar();
  // 主题由 SARibbonMainWindow 提供（不是 SARibbonBar 的接口）。它与应用级
  // QSS 是两套样式体系：功能区内部由 SARibbon 自己的样式表绘制，default.qss
  // 里的通用控件规则不参与
  setRibbonTheme(SARibbonTheme::RibbonThemeOffice2013);

  SARibbonCategory* home = ribbon->addCategoryPage(QStringLiteral("主页"));
  home->setObjectName(QStringLiteral("ribbonHomeCategory"));

  SARibbonPanel* panel_tool = home->addPanel(QStringLiteral("工具"));
  panel_tool->setObjectName(QStringLiteral("ribbonPanelTool"));
  panel_tool->addLargeAction(log_action_);
  panel_tool->addLargeAction(settings_action_);

  SARibbonPanel* panel_help = home->addPanel(QStringLiteral("帮助"));
  panel_help->setObjectName(QStringLiteral("ribbonPanelHelp"));
  panel_help->addLargeAction(about_action_);

  SARibbonPanel* panel_file = home->addPanel(QStringLiteral("文件"));
  panel_file->setObjectName(QStringLiteral("ribbonPanelFile"));
  panel_file->addLargeAction(quit_action_);
}
#else
// 普通菜单栏：BUILD_RIBBON=OFF 时的入口形态
void MainWindow::initMenuBar() {
  auto* file_menu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
  file_menu->setObjectName(QStringLiteral("menuFile"));
  file_menu->addAction(quit_action_);

  auto* tool_menu = menuBar()->addMenu(QStringLiteral("工具(&T)"));
  tool_menu->setObjectName(QStringLiteral("menuTool"));
  tool_menu->addAction(log_action_);
  tool_menu->addAction(settings_action_);

  auto* help_menu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
  help_menu->setObjectName(QStringLiteral("menuHelp"));
  help_menu->addAction(about_action_);
}
#endif

// 占位中央控件：模板使用者替换为自己的业务界面
void MainWindow::initCentralWidget() {
  auto* central = new QWidget(this);
  auto* layout = new QVBoxLayout(central);

  auto* title = new QLabel(QStringLiteral("主界面区域"), central);
  title->setObjectName(QStringLiteral("pagePlaceholderTitle"));
  title->setAlignment(Qt::AlignCenter);

  auto* hint = new QLabel(
      QStringLiteral("在此替换为你的业务界面（入口见上方功能区或菜单栏）"),
      central);
  hint->setObjectName(QStringLiteral("pagePlaceholderHint"));
  hint->setAlignment(Qt::AlignCenter);

  layout->addStretch();
  layout->addWidget(title);
  layout->addWidget(hint);
  layout->addStretch();

  setCentralWidget(central);
}

void MainWindow::initSignals() {}

void MainWindow::onShowLogWindow() {
  // 按需创建：不使用日志窗口的项目不会付出构造成本
  if (!log_window_) {
    log_window_ = new LogWindow(this);
  }
  log_window_->show();
  log_window_->raise();
  log_window_->activateWindow();
}

void MainWindow::onShowSettings() {
  SettingsDialog dialog(this);
  dialog.exec();
}

void MainWindow::onShowAbout() {
  AboutDialog dialog(this);
  dialog.exec();
}
