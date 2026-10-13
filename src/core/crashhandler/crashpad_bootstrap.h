#ifndef FILE_CORE_CRASHHANDLER_CRASHPAD_BOOTSTRAP_H_
#define FILE_CORE_CRASHHANDLER_CRASHPAD_BOOTSTRAP_H_

#include <QString>
#include <QVector>

#include "crashhandler/crash.h"

namespace crash {
namespace detail {

// 启动 crashpad_handler，并把崩溃写进 db_dir。产品名/版本/git 提交等以
// handler 注解的形式随 dump 走
bool InitCrashpad(const QString& db_dir, const QString& handler_path,
                  const AppInfo& info);

// 列出 db_dir 里待处理的报告（与 crash::ListPendingReports 同一契约）
QVector<PendingReport> ListPendingReports(const QString& db_dir);

// 导出并交付（与 crash::ExportPendingReports 同一契约）
ExportResult ExportPendingReports(const QString& db_dir,
                                  const QString& target_dir,
                                  const AppInfo& info);

}  // namespace detail
}  // namespace crash

#endif  // FILE_CORE_CRASHHANDLER_CRASHPAD_BOOTSTRAP_H_
