#ifndef FILE_CORE_CRASHHANDLER_CRASH_BOOTSTRAP_H_
#define FILE_CORE_CRASHHANDLER_CRASH_BOOTSTRAP_H_

#include <QString>

namespace crash {

// 初始化崩溃捕获。两套实现由 CMake 宏 APP_ENABLE_CRASHPAD 编译期切换：
//   ON  → Crashpad（进程外捕获，生成 minidump）
//   OFF → 自研（信号 / SEH，写 .log + .dmp）
//
// 崩溃文件统一落在：
//   Windows: %LOCALAPPDATA%/<Org>/<App>/crash/
//   Linux:   ~/.local/share/<Org>/<App>/crash/
//
// handlerPath 只在 Crashpad 模式下使用，指向 crashpad_handler(.exe)；
// 自研模式忽略。appName / appVersion 写入 minidump annotation，自研模式忽略。
//
// 返回 true 表示初始化成功。失败时调用方可提示用户，但不影响主流程。
bool init(const QString& handlerPath, const QString& appName,
          const QString& appVersion);

}  // namespace crash

#endif  // FILE_CORE_CRASHHANDLER_CRASH_BOOTSTRAP_H_