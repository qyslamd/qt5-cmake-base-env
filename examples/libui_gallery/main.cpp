#include <QApplication>

#include "config/config_manager.h"
#include "gallery_window.h"
#include "logger/logger.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("libui_gallery"));

  // 日志面板挂在 Logger 的 Qt sink 上，故必须先行初始化，顺序与
  // src/app/main.cpp 一致（ConfigManager 又要先于 Logger，因为日志配置
  // 是从配置里读的）。
  ConfigManager::instance();
  Logger::init();
  LOG_INFO("GALLERY", "libui 部件演示启动");

  GalleryWindow window;
  window.show();

  const int ret = app.exec();

  LOG_INFO("GALLERY", "libui 部件演示退出");
  Logger::shutdown();
  return ret;
}
