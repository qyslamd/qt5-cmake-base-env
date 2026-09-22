#include "mainwindow.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QVBoxLayout>

#include "dialogs/about_dialog.h"
#include "dialogs/settings_dialog.h"
#include "log_window.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  initUi();
  initSignals();
}

MainWindow::~MainWindow() {}

void MainWindow::initUi() {
  setWindowTitle(QCoreApplication::applicationName());
  setMinimumSize(800, 600);

  // objectName 供功能引导与 funq 按名字定位。
  // 动作也要名字：不给的话 funq 会拿"类名-序号"当路径（QMenu-1::QAction-2），
  // 那种合成名字既不稳定、引导侧也解析不了（它按 objectName 找）
  auto* file_menu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
  file_menu->setObjectName(QStringLiteral("menuFile"));
  auto* quit_action = file_menu->addAction(QStringLiteral("退出(&Q)"), this,
                                           &QWidget::close, QKeySequence::Quit);
  quit_action->setObjectName(QStringLiteral("actionQuit"));

  auto* tool_menu = menuBar()->addMenu(QStringLiteral("工具(&T)"));
  tool_menu->setObjectName(QStringLiteral("menuTool"));
  auto* log_action = tool_menu->addAction(QStringLiteral("日志窗口(&L)"), this,
                                          &MainWindow::onShowLogWindow);
  log_action->setObjectName(QStringLiteral("actionLogWindow"));
  auto* settings_action = tool_menu->addAction(QStringLiteral("设置(&S)"), this,
                                               &MainWindow::onShowSettings);
  settings_action->setObjectName(QStringLiteral("actionSettings"));

  auto* help_menu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
  help_menu->setObjectName(QStringLiteral("menuHelp"));
  auto* about_action = help_menu->addAction(QStringLiteral("关于(&A)"), this,
                                            &MainWindow::onShowAbout);
  about_action->setObjectName(QStringLiteral("actionAbout"));

  // 占位中央控件：模板使用者替换为自己的业务界面
  auto* central = new QWidget(this);
  auto* layout = new QVBoxLayout(central);

  auto* title = new QLabel(QStringLiteral("主界面区域"), central);
  title->setObjectName(QStringLiteral("pagePlaceholderTitle"));
  title->setAlignment(Qt::AlignCenter);

  auto* hint = new QLabel(
      QStringLiteral("在此替换为你的业务界面（菜单栏见「工具」与「帮助」）"),
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
