#include "crashhandler/crash_bootstrap.h"

#include <QDir>
#include <QStandardPaths>

#ifdef APP_ENABLE_CRASHPAD
#include "crashhandler/crashpad_bootstrap.h"
#else
#include <memory>

#include "crashhandler/crash_handler.h"

#endif

namespace crash {

namespace {

QString resolveDumpDir() {
  const QString base =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  const QString dir = base + QStringLiteral("/crash");
  QDir().mkpath(dir);
  return dir;
}

#ifndef APP_ENABLE_CRASHPAD
// 自研方案：handler 对象必须活到进程结束，否则析构时会把信号处理器撤掉。
// 用文件作用域静态变量持有，随进程退出自动析构（与 main 里局部持有的行为一致）
std::unique_ptr<CrashHandler> g_crash_handler;
#endif

}  // namespace

bool init(const QString& handlerPath, const QString& appName,
          const QString& appVersion) {
  const QString dumpDir = resolveDumpDir();

#ifdef APP_ENABLE_CRASHPAD
  return detail::initCrashpad(dumpDir, handlerPath, appName, appVersion);
#else
  Q_UNUSED(handlerPath);
  Q_UNUSED(appName);
  Q_UNUSED(appVersion);

  g_crash_handler = CrashHandler::create();
  if (!g_crash_handler) {
    return false;
  }
  g_crash_handler->setDumpPath(dumpDir);
  return g_crash_handler->init();
#endif
}

}  // namespace crash