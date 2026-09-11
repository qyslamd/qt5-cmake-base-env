#ifndef FILE_CORE_CRASHHANDLER_CRASH_HANDLER_H_
#define FILE_CORE_CRASHHANDLER_CRASH_HANDLER_H_

#include <functional>
#include <memory>

#include <QString>

class CrashHandler {
 public:
  static std::unique_ptr<CrashHandler> create();
  virtual ~CrashHandler() = default;

  virtual bool init() = 0;
  virtual void setDumpPath(const QString& path) = 0;
  virtual void setCrashCallback(
      std::function<void(const QString& crashLog)> callback) = 0;

  // 测试用公共接口
  QString generateCrashFileName() const;
  QString collectCommonInfo() const;

 protected:
  CrashHandler() = default;
};

#endif  // FILE_CORE_CRASHHANDLER_CRASH_HANDLER_H_
