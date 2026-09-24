#include "crashhandler/crashpad_bootstrap.h"

#include <map>
#include <string>
#include <vector>

#include <QDir>

#include "client/crash_report_database.h"
#include "client/crashpad_client.h"
#include "client/settings.h"

namespace crash {
namespace detail {

namespace {

// QString → base::FilePath：Windows 下 FilePath 用 std::wstring，POSIX 下用
// std::string。_WIN32 是编译器预定义宏，不依赖 Qt 头
base::FilePath toFilePath(const QString& s) {
#ifdef _WIN32
  return base::FilePath(s.toStdWString());
#else
  return base::FilePath(s.toStdString());
#endif
}

}  // namespace

bool initCrashpad(const QString& dumpDir, const QString& handlerPath,
                  const QString& appName, const QString& appVersion) {
  QDir().mkpath(dumpDir);

  const base::FilePath handler = toFilePath(handlerPath);
  const base::FilePath db = toFilePath(dumpDir);

  // 不上传：url 留空，dump 仅存本地
  const std::string url;

  std::map<std::string, std::string> annotations;
  annotations["format"] = "minidump";
  annotations["product"] = appName.toStdString();
  annotations["version"] = appVersion.toStdString();

  // 关闭限流，保证每次崩溃都有 dump
  std::vector<std::string> arguments;
  arguments.push_back("--no-rate-limit");

  auto database = ::crashpad::CrashReportDatabase::Initialize(db);
  if (!database) {
    return false;
  }

  auto* settings = database->GetSettings();
  if (!settings) {
    return false;
  }
  settings->SetUploadsEnabled(false);

  ::crashpad::CrashpadClient client;
  return client.StartHandler(
      handler,
      db,  // database（reports）
      db,  // metrics
      url, annotations, arguments,
      true,   // restartable
      false,  // asynchronous_start：同步启动，失败即刻返回
      {});    // attachments
}

}  // namespace detail
}  // namespace crash