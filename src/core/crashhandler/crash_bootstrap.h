#ifndef FILE_CORE_CRASHHANDLER_CRASH_BOOTSTRAP_H_
#define FILE_CORE_CRASHHANDLER_CRASH_BOOTSTRAP_H_

#include <QString>
#include <QVector>

namespace crash {

// 应用元信息：写进 minidump 的注解（随 dump 走），导出时也写进 summary.txt。
// core 层拿不到 src/app 生成的 AppVersion.h，故由调用方传进来。
struct AppInfo {
  QString name;          // 应用名（CRASHPAD product）
  QString version;       // 形如 V0.1.0.123
  QString git_commit;    // 归档目录名里的那个短 hash
  QString build_config;  // Debug / RelWithDebInfo ...
};

// 一条待处理的崩溃报告。uuid 是报告在库内的唯一稳定标识——不要用下标引用
// 报告：GetPendingReports() 每次都返回新快照，导出/转移后下标会错位
struct PendingReport {
  QString uuid;
  qint64 created_at = 0;  // 崩溃时间（Unix 秒）
  qint64 size = 0;        // 报告文件字节数
};

// 崩溃捕获后端。三个实现都编译在程序里，运行时按配置（crash/backend，
// 重启生效）挑一个装
enum class Backend {
  kCrashpad,  // 进程外捕获（crashpad_handler 独立进程 + minidump），默认
  kBuiltin,   // 自研（信号 / SEH，写 .log + .dmp，进程内解析符号）
  kNone,      // 不装捕获，只记应用元信息
};

// 初始化崩溃捕获，按 backend 装对应的实现。每次进程只调一次，后端随之定死
//
// 崩溃文件统一落在：
//   Windows: %LOCALAPPDATA%/<Org>/<App>/crash/
//   Linux:   ~/.local/share/<Org>/<App>/crash/
//
// handler_path 只在 kCrashpad 下使用，指向 crashpad_handler(.exe)；其余后端
// 忽略。返回 true 表示初始化成功，失败不影响主流程。
bool Init(const QString& handler_path, const AppInfo& info, Backend backend);

// 本应用实际使用的崩溃数据库目录（与 Init 用的是同一个）
QString DatabaseDir();

// Init 时记录的应用元信息；未初始化时字段为空
const AppInfo& CurrentAppInfo();

// 列出待处理的崩溃报告。库打不开（首次运行）时返回空表——这不是错误，
// 是"没有报告"。仅 kCrashpad 后端有报告库，其余后端恒返回空表
QVector<PendingReport> ListPendingReports(const QString& db_dir);

struct ExportResult {
  int exported = 0;    // 成功导出的报告数
  int failed = 0;      // 拷贝失败的份数（仍留在库里，下次再导）
  QString target_dir;  // 实际落盘目录
  QString error;       // 空 = 本次调用成功（不代表有报告）
  bool ok() const { return error.isEmpty(); }
};

// 导出全部待处理报告到 target_dir：每份一个 <uuid>.dmp，外加一份人可读的
// summary.txt。拷贝并校验通过的报告转入 completed（不删除，见设计文档 D5）。
// 仅 kCrashpad 后端有报告库，其余后端返回"成功、零份"。
//
// db_dir / target_dir / info 全部由调用方给出，就是为了让单元测试指向临时目录，
// 不碰开发者本机真实的崩溃库。
ExportResult ExportPendingReports(const QString& db_dir,
                                  const QString& target_dir,
                                  const AppInfo& info);

}  // namespace crash

#endif  // FILE_CORE_CRASHHANDLER_CRASH_BOOTSTRAP_H_
