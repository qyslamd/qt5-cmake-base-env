#ifndef ETEST_CORE_CRASHHANDLER_CRASHHANDLER_H_
#define ETEST_CORE_CRASHHANDLER_CRASHHANDLER_H_

#include <QString>
#include <functional>
#include <memory>


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

#endif  // ETEST_CORE_CRASHHANDLER_CRASHHANDLER_H_
