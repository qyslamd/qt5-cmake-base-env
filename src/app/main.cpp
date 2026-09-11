#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QIcon>

#include "AppVersion.h"
#include "MainWindow.h"
#include "auth/auth_service.h"
#include "config/config_defs.h"
#include "config/config_manager.h"
#include "dialogs/login_dialog.h"
#include "logger/logger.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setOrganizationName("Enpht");
  app.setApplicationName("FILE");
  app.setWindowIcon(QIcon(":/resources/icons/icon.ico"));
  app.setApplicationVersion(getProjectVersionAll());

  // 加载全局样式
  QFile style_file(QStringLiteral(":/resources/styles/default.qss"));
  if (style_file.open(QIODevice::ReadOnly)) {
    app.setStyleSheet(QString::fromUtf8(style_file.readAll()));
  }

  // 配置管理必须先于 Logger 初始化（Logger 读取日志配置）
  ConfigManager::instance();

  Logger::init();
  LOG_INFO("MAIN", "应用程序启动，版本: {}", getProjectVersionAll());

  MainWindow w;
  // 钳制窗口尺寸，防止损坏的 INI 产生不可用窗口
  const int saved_width = ConfigManager::instance().get<int>(
      QString::fromLatin1(kConfigWindowWidth), kConfigWindowDefaultWidth);
  const int saved_height = ConfigManager::instance().get<int>(
      QString::fromLatin1(kConfigWindowHeight), kConfigWindowDefaultHeight);
  w.resize(qBound(640, saved_width, 4096), qBound(480, saved_height, 2160));

  // 登录成功后才显示主窗口
  LoginDialog login(&w);
  if (login.exec() != QDialog::Accepted) {
    LOG_INFO("MAIN", "未登录，退出程序");
    Logger::shutdown();
    return 0;
  }
  LOG_INFO("MAIN", "登录成功，当前用户: {}",
           AuthService::instance().currentUser().user_name_.toStdString());
  w.show();

  // 退出前保存窗口尺寸并立即落盘
  QObject::connect(&app, &QApplication::aboutToQuit, [&w]() {
    auto& cfg = ConfigManager::instance();
    cfg.set<int>(QString::fromLatin1(kConfigWindowWidth), w.width());
    cfg.set<int>(QString::fromLatin1(kConfigWindowHeight), w.height());
    cfg.sync();
  });

  int ret = app.exec();

  LOG_INFO("MAIN", "应用程序退出");
  Logger::shutdown();
  return ret;
}
