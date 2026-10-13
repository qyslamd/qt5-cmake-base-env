#include "crashhandler/crash_bootstrap.h"

#include <memory>

#include <QDir>
#include <QStandardPaths>

#include "crashhandler/crash_handler.h"
#include "crashhandler/crashpad_bootstrap.h"

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

// 本次运行选定的后端。查询接口据此决定读真库还是按"没有报告"处理；默认
// kCrashpad，让单元测试不经过 Init 也能直接演练导出
Backend g_backend = Backend::kCrashpad;

// 自研方案：handler 对象必须活到进程结束，否则析构时会把信号处理器撤掉。
// 用文件作用域静态变量持有，随进程退出自动析构
std::unique_ptr<CrashHandler> g_crash_handler;

}  // namespace

QString DatabaseDir() {
  if (g_db_dir.isEmpty()) {
    g_db_dir = ResolveDumpDir();
  }
  return g_db_dir;
}

const AppInfo& CurrentAppInfo() { return g_app_info; }

bool Init(const QString& handler_path, const AppInfo& info, Backend backend) {
  g_app_info = info;
  g_backend = backend;
  const QString db_dir = DatabaseDir();

  switch (backend) {
    case Backend::kCrashpad:
      return detail::InitCrashpad(db_dir, handler_path, info);
    case Backend::kBuiltin:
      g_crash_handler = CrashHandler::create();
      if (g_crash_handler == nullptr) {
        return false;
      }
      g_crash_handler->setDumpPath(db_dir);
      return g_crash_handler->init();
    case Backend::kNone:
      // 没有后端要装，但应用元信息与数据库目录照记（导出接口要读它们，
      // 只是那两处会按"没有报告"处理）。返回 true = 本次没有失败
      return true;
  }
  return true;
}

QVector<PendingReport> ListPendingReports(const QString& db_dir) {
  if (g_backend != Backend::kCrashpad) {
    // 自研/关闭模式没有 crashpad 报告库
    Q_UNUSED(db_dir);
    return {};
  }
  return detail::ListPendingReports(db_dir);
}

ExportResult ExportPendingReports(const QString& db_dir,
                                  const QString& target_dir,
                                  const AppInfo& info) {
  if (g_backend != Backend::kCrashpad) {
    // 自研/关闭模式没有报告库：不是错误，就是"没有报告"
    Q_UNUSED(db_dir);
    Q_UNUSED(target_dir);
    Q_UNUSED(info);
    return ExportResult{};
  }
  return detail::ExportPendingReports(db_dir, target_dir, info);
}

}  // namespace crash
