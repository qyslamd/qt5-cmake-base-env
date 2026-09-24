#ifndef FILE_CORE_CRASHHANDLER_CRASHPAD_BOOTSTRAP_H_
#define FILE_CORE_CRASHHANDLER_CRASHPAD_BOOTSTRAP_H_

#include <QString>

namespace crash {
namespace detail {

bool initCrashpad(const QString& dumpDir, const QString& handlerPath,
                  const QString& appName, const QString& appVersion);

}  // namespace detail
}  // namespace crash

#endif  // FILE_CORE_CRASHHANDLER_CRASHPAD_BOOTSTRAP_H_