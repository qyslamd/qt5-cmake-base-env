#include "crashhandler/crash_bootstrap.h"

#ifndef APP_ENABLE_CRASHPAD
#include <memory>
#endif

#include <QDir>
#include <QStandardPaths>

#ifdef APP_ENABLE_CRASHPAD
#include "crashhandler/crashpad_bootstrap.h"
#else
#include "crashhandler/crash_handler.h"
#endif

namespace crash {

namespace {

QString ResolveDumpDir() {
  const QString base =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  const QString dir = base + QStringLiteral("/crash");
  QDir().mkpath(dir);
  return dir;
}

// Init 时定下来、之后不再变。查询接口与 Init 必须落在同一个目录：目录由
// 组织名/应用名派生（QStandardPaths），若某次调用发生在 setApplicationName
// 之前，算出来的就是另一个路径，导出会读到空的库里
QString g_db_dir;
AppInfo g_app_info;

#ifndef APP_ENABLE_CRASHPAD
// 自研方案：handler 对象必须活到进程结束，否则析构时会把信号处理器撤掉。
// 用文件作用域静态变量持有，随进程退出自动析构
std::unique_ptr<CrashHandler> g_crash_handler;
#endif

}  // namespace

QString DatabaseDir() {
  if (g_db_dir.isEmpty()) {
    g_db_dir = ResolveDumpDir();
  }
  return g_db_dir;
}

const AppInfo& CurrentAppInfo() { return g_app_info; }

bool Init(const QString& handler_path, const AppInfo& info) {
  g_app_info = info;
  const QString db_dir = DatabaseDir();

#ifdef APP_ENABLE_CRASHPAD
  return detail::InitCrashpad(db_dir, handler_path, info);
#else
  Q_UNUSED(handler_path);

  g_crash_handler = CrashHandler::create();
  if (!g_crash_handler) {
    return false;
  }
  g_crash_handler->setDumpPath(db_dir);
  return g_crash_handler->init();
#endif
}

QVector<PendingReport> ListPendingReports(const QString& db_dir) {
#ifdef APP_ENABLE_CRASHPAD
  return detail::ListPendingReports(db_dir);
#else
  Q_UNUSED(db_dir);
  return {};
#endif
}

ExportResult ExportPendingReports(const QString& db_dir,
                                  const QString& target_dir,
                                  const AppInfo& info) {
#ifdef APP_ENABLE_CRASHPAD
  return detail::ExportPendingReports(db_dir, target_dir, info);
#else
  Q_UNUSED(db_dir);
  Q_UNUSED(target_dir);
  Q_UNUSED(info);
  // 自研模式没有报告库：不是错误，就是"没有报告"
  return ExportResult{};
#endif
}

}  // namespace crash
