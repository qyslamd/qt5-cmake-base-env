#include "mainwindow.h"

#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QVBoxLayout>

// clang-format off
// 三条路线各自的入口形态不同，各自只包含自己那套头文件
#ifdef APP_UI_FLUENT
#include <QMenu>
#include <QMenuBar>
#include <fluentwindowframe.h>
#elif defined(APP_ENABLE_RIBBON)
#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonPanel.h>
#else
#include <QMenu>
#include <QMenuBar>
#endif
// clang-format on

#include "config/config_defs.h"
#include "config/config_manager.h"
#include "dialogs/about_dialog.h"
#include "dialogs/settings_dialog.h"
#include "log_window.h"

namespace {
QSize LoadWindowSize() {
  const int saved_width = ConfigManager::instance().get<int>(
      QString::fromLatin1(kConfigWindowWidth), kConfigWindowDefaultWidth);
  const int saved_height = ConfigManager::instance().get<int>(
      QString::fromLatin1(kConfigWindowHeight), kConfigWindowDefaultHeight);
  return QSize(qBound(640, saved_width, 4096), qBound(480, saved_height, 2160));
}

void SaveWindowSize(const QWidget& window) {
  auto& cfg = ConfigManager::instance();
  cfg.set<int>(QString::fromLatin1(kConfigWindowWidth), window.width());
  cfg.set<int>(QString::fromLatin1(kConfigWindowHeight), window.height());
  cfg.sync();
}
}  // namespace

MainWindow::MainWindow(QWidget* parent) : MainWindowBase(parent) {
  initUi();
  initSignals();
}

MainWindow::~MainWindow() {}

void MainWindow::initUi() {
  setWindowTitle(QCoreApplication::applicationName());

  // size
  setMinimumSize(800, 600);
  const QSize window_size = LoadWindowSize();
  resize(window_size);

  createActions();
#if defined(APP_UI_FLUENT)
  initMenuBar();
  initFluentFrame();
#elif defined(APP_ENABLE_RIBBON)
  initRibbon();
#else
  initMenuBar();
#endif
  initCentralWidget();
}

// 动作：objectName 供功能引导与 funq 按名字定位。
// 不给名字的话 funq 会拿「类名-序号」当路径（QAction-2 之类），那种合成名字
// 既不稳定、引导侧也解析不了（它按 objectName 找）
//
// 图标是彩色 SVG（`resources/icons/svg/`）：功能区形态下每个动作就是一个大
// 按钮，没有图标会是一排纯文字。两种形态共用这批动作，菜单栏形态跟着一起有
// 图标。SVG 由 Qt 的 svg 图片插件渲染（开发机随 Qt 安装，发布包由
// windeployqt 收进 imageformats/qsvg.dll，Linux 侧在 libqt5svg5 里）
void MainWindow::createActions() {
  quit_action_ = new QAction(QIcon(":/resources/icons/svg/quit.svg"),
                             QStringLiteral("退出(&Q)"), this);
  quit_action_->setObjectName(QStringLiteral("actionQuit"));
  quit_action_->setShortcut(QKeySequence::Quit);
  connect(quit_action_, &QAction::triggered, this, &QWidget::close);

  log_action_ = new QAction(QIcon(":/resources/icons/svg/log_window.svg"),
                            QStringLiteral("日志窗口(&L)"), this);
  log_action_->setObjectName(QStringLiteral("actionLogWindow"));
  connect(log_action_, &QAction::triggered, this, &MainWindow::onShowLogWindow);

  settings_action_ = new QAction(QIcon(":/resources/icons/svg/settings.svg"),
                                 QStringLiteral("设置(&S)"), this);
  settings_action_->setObjectName(QStringLiteral("actionSettings"));
  connect(settings_action_, &QAction::triggered, this,
          &MainWindow::onShowSettings);

  about_action_ = new QAction(QIcon(":/resources/icons/svg/about.svg"),
                              QStringLiteral("关于(&A)"), this);
  about_action_->setObjectName(QStringLiteral("actionAbout"));
  connect(about_action_, &QAction::triggered, this, &MainWindow::onShowAbout);
}

#ifdef APP_ENABLE_RIBBON
// 功能区：一个「主页」分类下挂三个面板。
// 面板也要 objectName——菜单项本身是 QAction（不是 QWidget），引导的高亮与
// funq 的定位都落在控件上，指不到动作，只能指它所在的面板
void MainWindow::initRibbon() {
  SARibbonBar* ribbon = ribbonBar();
  // 主题由 SARibbonMainWindow 提供（不是 SARibbonBar 的接口）：它在主窗口上
  // 设一份 widget 级样式表，只覆盖主题自己声明的属性。QSS 的级联规则是
  // "冲突时 widget 表优先"，所以应用级样式表（base/*.qss + default.qss）里
  // SARibbon 主题没声明的属性照样会落到功能区上——底座层那些裸类型规则
  // （QListView、QScrollBar 等）现在就在改功能区。改底座层要一并看功能区
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

#ifdef APP_UI_FLUENT
// Fluent 路线的窗口壳：QWK 无边框 + FluentTitleBar。菜单栏交给
// installChromeHeader——它内部走 setMenuWidget，把「标题栏 + 菜单栏」当成
// 窗口顶部的整体，窗口不会因此多占一行
void MainWindow::initFluentFrame() {
  fluent_frame_ = new FluentWindowFrame(this, this);
  fluent_frame_->installChromeHeader(menuBar());
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

void MainWindow::initSignals() {
  // 退出前保存窗口尺寸并立即落盘
  connect(qApp, &QCoreApplication::aboutToQuit,
          [this]() { SaveWindowSize(*this); });
}

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
