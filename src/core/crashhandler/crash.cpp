#include "crashhandler/crash.h"

#include <memory>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>

#include "config/config_defs.h"
#include "config/config_manager.h"
#include "crashhandler/crash_handler.h"
#include "crashhandler/crashpad_bootstrap.h"
#include "logger/logger.h"

namespace crash {

namespace {

// crash/backend 值 → 枚举；空/非法回落 crashpad 并告警，配置写坏不拦启动
Backend ParseBackend(const QString& value) {
  if (value.isEmpty() || value == QLatin1String(kCrashBackendCrashpad)) {
    return Backend::kCrashpad;
  }
  if (value == QLatin1String(kCrashBackendBuiltin)) {
    return Backend::kBuiltin;
  }
  if (value == QLatin1String(kCrashBackendNone)) {
    return Backend::kNone;
  }
  LOG_WARN("CRASH", "crash/backend 配置非法：{}，回落 crashpad",
           value.toStdString());
  return Backend::kCrashpad;
}

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

Backend CurrentBackend() { return g_backend; }

bool Init(const AppInfo& info) {
  g_app_info = info;
  const Backend backend = ParseBackend(ConfigManager::instance().get<QString>(
      QString::fromLatin1(kConfigCrashBackend),
      QString::fromLatin1(kCrashBackendCrashpad)));
  g_backend = backend;
  const QString db_dir = DatabaseDir();

  bool ok = false;
  switch (backend) {
    case Backend::kCrashpad: {
      // handler 与主程序同目录（构建系统保证落位）
      const QString exe_dir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
      const QString handler_path =
          exe_dir + QStringLiteral("/crashpad_handler.exe");
#else
      const QString handler_path =
          exe_dir + QStringLiteral("/crashpad_handler");
#endif
      ok = detail::InitCrashpad(db_dir, handler_path, info);
      break;
    }
    case Backend::kBuiltin:
      g_crash_handler = CrashHandler::create();
      if (g_crash_handler == nullptr) {
        break;
      }
      g_crash_handler->setDumpPath(db_dir);
      ok = g_crash_handler->init();
      break;
    case Backend::kNone:
      // 没有后端要装，但应用元信息与数据库目录照记（导出接口要读它们，
      // 只是那两处会按"没有报告"处理）。返回 true = 本次没有失败
      ok = true;
      break;
  }

  const char* name = backend == Backend::kCrashpad  ? "Crashpad"
                     : backend == Backend::kBuiltin ? "自研"
                                                    : "none";
  if (backend == Backend::kNone) {
    LOG_INFO("CRASH", "未启用崩溃捕获（crash/backend=none）");
  } else if (ok) {
    LOG_INFO("CRASH", "崩溃捕获模块初始化完成（{}）", name);
  } else {
    LOG_WARN("CRASH", "崩溃捕获初始化失败（{}），本次运行不会生成崩溃报告",
             name);
  }
  return ok;
}

QVector<PendingReport> ListPendingReports(const QString& db_dir) {
  if (g_backend != Backend::kCrashpad) {
    // 自研/关闭模式没有 crashpad 报告库
    Q_UNUSED(db_dir);
    return {};
  }
  return detail::ListPendingReports(db_dir);
}

QString DefaultExportDir() {
  // 桌面 → 文档 → 主目录：客户机上"桌面"最容易被找到，但无桌面环境（或精简
  // 系统）可能没有它，逐级退到一定存在的目录
  const QStandardPaths::StandardLocation locations[] = {
      QStandardPaths::DesktopLocation, QStandardPaths::DocumentsLocation,
      QStandardPaths::HomeLocation};
  QString base;
  for (QStandardPaths::StandardLocation location : locations) {
    const QString path = QStandardPaths::writableLocation(location);
    if (!path.isEmpty() && QDir(path).exists()) {
      base = path;
      break;
    }
  }
  if (base.isEmpty()) {
    base = QDir::homePath();
  }

  const QString stamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
  return base + QStringLiteral("/CrashReport-") + stamp;
}

ExportResult ExportTo(const QString& target_dir) {
  const QString dir = target_dir.isEmpty() ? DefaultExportDir() : target_dir;
  return ExportPendingReports(DatabaseDir(), dir, CurrentAppInfo());
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
