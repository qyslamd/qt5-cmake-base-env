#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QIcon>

#include "AppInfo.h"
#include "AppVersion.h"
#include "auth/auth_service.h"
#include "common/GlobalExceptionHandler.h"
#include "common/SingleInstance.h"
#include "config/config_defs.h"
#include "config/config_manager.h"
#include "crashhandler/CrashHandler.h"
#include "dialogs/login_dialog.h"
#include "logger/logger.h"
#include "mainwindow.h"
#include "splash_screen.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  // 组织名/应用名来自 CMake，决定 %LOCALAPPDATA% 数据目录、单实例键与
  // 日志文件名，须与安装脚本的卸载路径保持一致
  app.setOrganizationName(QString::fromLatin1(APP_ORGANIZATION_NAME));
  app.setApplicationName(QString::fromLatin1(APP_APPLICATION_NAME));
  app.setWindowIcon(QIcon(":/resources/icons/icon.ico"));
  app.setApplicationVersion(getProjectVersionAll());

  // 单实例检测：appKey 随 applicationName 走，互斥体名由 CMake 提供
  // （与安装脚本同源），第二实例转发参数后退出
  SingleInstance single_instance(QCoreApplication::applicationName(),
                                 QString::fromLatin1(APP_MUTEX_NAME));
  if (single_instance.isAppAlreadyRunning()) {
    single_instance.connectToExistingInstance(QCoreApplication::arguments());
    return 0;
  }
  single_instance.startListening();

  // 加载全局样式
  QFile style_file(QStringLiteral(":/resources/styles/default.qss"));
  if (style_file.open(QIODevice::ReadOnly)) {
    app.setStyleSheet(QString::fromUtf8(style_file.readAll()));
  }

  // 配置管理必须先于 Logger 初始化（Logger 读取日志配置）
  ConfigManager::instance();

  Logger::init();
  LOG_INFO("MAIN", "应用程序启动，版本: {}", getProjectVersionAll());

  // 信号捕获 + Qt 消息重定向。致命信号实际由 CrashHandler 接管
  // （其注册晚于本模块），此处生效的是 Qt 消息重定向与 SIGINT/SIGTERM
  GlobalExceptionHandler::instance().init();

  // 初始化崩溃捕获模块
  auto crash_handler = CrashHandler::create();
  if (crash_handler) {
    crash_handler->init();
    LOG_INFO("MAIN", "崩溃捕获模块初始化完成");
  }

  // 登录成功前不创建主窗口
  LoginDialog login;
  if (login.exec() != QDialog::Accepted) {
    LOG_INFO("MAIN", "未登录，退出程序");
    GlobalExceptionHandler::instance().shutdown();
    Logger::shutdown();
    return 0;
  }
  LOG_INFO("MAIN", "登录成功，当前用户: {}",
           AuthService::instance().currentUser().user_name_.toStdString());

  // 启动阶段：Splash 覆盖主界面创建过程。各步骤之间插入
  // processEvents 保证进度条实时刷新；模板使用者可在步骤之间加入
  // 自己的耗时初始化逻辑，Splash 无需感知主窗口内部结构。
  SplashScreen splash;
  splash.show();
  QCoreApplication::processEvents();

  splash.setStatusText(QStringLiteral("初始化用户会话"));
  splash.setProgress(25);
  QCoreApplication::processEvents();
  const User current_user = AuthService::instance().currentUser();
  LOG_INFO("MAIN", "用户会话已建立: {}", current_user.user_name_.toStdString());

  splash.setStatusText(QStringLiteral("加载用户配置"));
  splash.setProgress(50);
  QCoreApplication::processEvents();
  // 钳制窗口尺寸，防止损坏的 INI 产生不可用窗口
  const int saved_width = ConfigManager::instance().get<int>(
      QString::fromLatin1(kConfigWindowWidth), kConfigWindowDefaultWidth);
  const int saved_height = ConfigManager::instance().get<int>(
      QString::fromLatin1(kConfigWindowHeight), kConfigWindowDefaultHeight);
  const int window_width = qBound(640, saved_width, 4096);
  const int window_height = qBound(480, saved_height, 2160);

  splash.setStatusText(QStringLiteral("创建主界面"));
  splash.setProgress(80);
  QCoreApplication::processEvents();
  MainWindow w;
  w.resize(window_width, window_height);

  splash.setStatusText(QStringLiteral("启动完成"));
  splash.setProgress(100);
  QCoreApplication::processEvents();
  splash.finish();

  w.show();

  // 第二实例请求激活：Windows 在 SingleInstance 内走 HWND 置前，
  // 其余平台依赖本信号完成 Qt 层的显示与激活
  single_instance.setActivationWindow(reinterpret_cast<void*>(w.winId()));
  QObject::connect(&single_instance, &SingleInstance::showApplication, &w,
                   [&w]() {
                     w.showNormal();
                     w.raise();
                     w.activateWindow();
                   });

  // 退出前保存窗口尺寸并立即落盘
  QObject::connect(&app, &QApplication::aboutToQuit, [&w]() {
    auto& cfg = ConfigManager::instance();
    cfg.set<int>(QString::fromLatin1(kConfigWindowWidth), w.width());
    cfg.set<int>(QString::fromLatin1(kConfigWindowHeight), w.height());
    cfg.sync();
  });

  int ret = app.exec();

  GlobalExceptionHandler::instance().shutdown();
  LOG_INFO("MAIN", "应用程序退出");
  Logger::shutdown();
  return ret;
}
