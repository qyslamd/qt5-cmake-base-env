#include "crashhandler/crash_bootstrap.h"

#ifdef APP_ENABLE_CRASH_CAPTURE
#include <memory>
#endif

#include <QDir>
#include <QStandardPaths>

#if defined(APP_ENABLE_CRASHPAD)
#include "crashhandler/crashpad_bootstrap.h"
#elif defined(APP_ENABLE_CRASH_CAPTURE)
#include "crashhandler/crash_handler.h"
#endif
// none：两个后端都不编，也就不 include 任何后端头

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

#ifdef APP_ENABLE_CRASH_CAPTURE
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

#if defined(APP_ENABLE_CRASHPAD)
  return detail::InitCrashpad(db_dir, handler_path, info);
#elif defined(APP_ENABLE_CRASH_CAPTURE)
  Q_UNUSED(handler_path);

  g_crash_handler = CrashHandler::create();
  if (!g_crash_handler) {
    return false;
  }
  g_crash_handler->setDumpPath(db_dir);
  return g_crash_handler->init();
#else
  // none：没有后端要装，但应用元信息与数据库目录照记（导出接口要读它们，
  // 只是那两处会返回"没有报告"）。返回 true = 本次没有失败
  Q_UNUSED(handler_path);
  return true;
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
