#include "crashhandler/crashpad_bootstrap.h"

#include <map>
#include <string>
#include <vector>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSysInfo>
#include <QTextStream>

#include "client/crash_report_database.h"
#include "client/crashpad_client.h"
#include "client/settings.h"
#include "util/misc/metrics.h"

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

QString toQString(const base::FilePath& path) {
#ifdef _WIN32
  return QString::fromStdWString(path.value());
#else
  return QString::fromStdString(path.value());
#endif
}

QString platformName() {
#if defined(Q_OS_WIN)
  return QStringLiteral("Windows");
#elif defined(Q_OS_LINUX)
  return QStringLiteral("Linux");
#else
  return QStringLiteral("Unknown");
#endif
}

QString formatTime(qint64 seconds) {
  return QDateTime::fromSecsSinceEpoch(seconds)
      .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

// summary.txt 的内容：开发者在打开 dump 之前先看它。
//
// 注解在 dump 里也有一份，但落在 CrashpadInfo 扩展流里，cdb/WinDbg 看不见，
// 所以这里再给一份人可读的副本
QString BuildSummary(const AppInfo& info,
                     const QVector<PendingReport>& exported) {
  // 全部走 QString 字面量：QTextStream 的 operator<<(const char*) 会把窄字面量
  // 按 Latin-1 解释，中文会变成"UTF-8 字节被再编码一次"的乱码
  QString text;
  QTextStream out(&text);
  out << QStringLiteral("应用:      ") << info.name << QLatin1Char('\n');
  out << QStringLiteral("版本:      ") << info.version << QLatin1Char('\n');
  out << QStringLiteral("git 提交:  ") << info.git_commit
      << QLatin1Char('\n');
  out << QStringLiteral("构建配置:  ") << info.build_config
      << QLatin1Char('\n');
  out << QStringLiteral("平台:      ") << platformName()
      << QStringLiteral(" / ") << QSysInfo::currentCpuArchitecture()
      << QLatin1Char('\n');
  out << QStringLiteral("导出时间:  ")
      << QDateTime::currentDateTime().toString(
             QStringLiteral("yyyy-MM-dd HH:mm:ss"))
      << QStringLiteral("\n\n");
  out << QStringLiteral("崩溃报告（") << exported.size()
      << QStringLiteral(" 份）:") << QLatin1Char('\n');
  for (const PendingReport& report : exported) {
    out << QStringLiteral("  ") << report.uuid << QStringLiteral(".dmp    ")
        << (report.size / 1024) << QStringLiteral(" KB    崩溃时间 ")
        << formatTime(report.created_at) << QLatin1Char('\n');
  }
  return text;
}

// 写 summary.txt。带 UTF-8 BOM：这份文件是给人看的，可能被 Windows 记事本
// 直接打开，BOM 能免掉"编码猜错变乱码"这一类问题
bool WriteSummary(const QString& path, const QString& text) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  if (file.write("\xEF\xBB\xBF", 3) != 3) {
    return false;
  }
  QTextStream out(&file);
  out.setCodec("UTF-8");
  out << text;
  out.flush();
  return file.error() == QFile::NoError;
}

}  // namespace

bool InitCrashpad(const QString& db_dir, const QString& handler_path,
                  const AppInfo& info) {
  QDir().mkpath(db_dir);

  const base::FilePath handler = toFilePath(handler_path);
  const base::FilePath db = toFilePath(db_dir);

  // 不上传：url 留空，dump 仅存本地
  const std::string url;

  // 注解随 dump 走（handler 侧写进 minidump），供事后核对"这份崩溃是哪个构建"
  std::map<std::string, std::string> annotations;
  annotations["format"] = "minidump";
  annotations["product"] = info.name.toStdString();
  annotations["version"] = info.version.toStdString();
  annotations["git"] = info.git_commit.toStdString();
  annotations["build"] = info.build_config.toStdString();
  annotations["os"] = platformName().toStdString();
  annotations["arch"] = QSysInfo::currentCpuArchitecture().toStdString();

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
      db,  // database
      db,  // metrics
      url, annotations, arguments,
      true,   // restartable
      false,  // asynchronous_start：同步启动，失败即刻返回
      {});    // attachments
}

QVector<PendingReport> ListPendingReports(const QString& db_dir) {
  QVector<PendingReport> result;

  // 库打不开（首次运行还没建库）不是错误，就是"没有报告"
  auto database =
      ::crashpad::CrashReportDatabase::Initialize(toFilePath(db_dir));
  if (!database) {
    return result;
  }

  std::vector<::crashpad::CrashReportDatabase::Report> reports;
  if (database->GetPendingReports(&reports) !=
      ::crashpad::CrashReportDatabase::kNoError) {
    // 与别的实例抢锁（Windows 下取库是加锁的）时按"这轮没有"处理
    return result;
  }

  for (const auto& report : reports) {
    PendingReport item;
    item.uuid = QString::fromStdString(report.uuid.ToString());
    item.created_at = static_cast<qint64>(report.creation_time);
    item.size = static_cast<qint64>(report.total_size);
    result.push_back(item);
  }
  return result;
}

ExportResult ExportPendingReports(const QString& db_dir,
                                  const QString& target_dir,
                                  const AppInfo& info) {
  using Report = ::crashpad::CrashReportDatabase::Report;

  ExportResult result;
  result.target_dir = target_dir;

  if (!QDir().mkpath(target_dir)) {
    result.error = QStringLiteral("无法创建导出目录：%1").arg(target_dir);
    return result;
  }

  auto database =
      ::crashpad::CrashReportDatabase::Initialize(toFilePath(db_dir));
  if (!database) {
    result.error = QStringLiteral("无法打开崩溃报告库：%1").arg(db_dir);
    return result;
  }

  std::vector<Report> reports;
  const auto status = database->GetPendingReports(&reports);
  if (status == ::crashpad::CrashReportDatabase::kBusyError) {
    // 另一实例正在用同一个库：本轮什么也不做，不当作失败
    return result;
  }
  if (status != ::crashpad::CrashReportDatabase::kNoError) {
    result.error = QStringLiteral("读取崩溃报告列表失败（状态 %1）")
                       .arg(static_cast<int>(status));
    return result;
  }

  QVector<PendingReport> exported;
  for (const Report& report : reports) {
    const QString uuid = QString::fromStdString(report.uuid.ToString());
    const QString source = toQString(report.file_path);
    const QString destination =
        target_dir + QLatin1Char('/') + uuid + QStringLiteral(".dmp");

    const qint64 source_size = QFileInfo(source).size();
    if (source_size <= 0) {
      ++result.failed;
      continue;
    }

    // 同一份报告重复导出时覆盖旧文件：文件名就是 uuid，天然幂等
    QFile::remove(destination);
    if (!QFile::copy(source, destination) ||
        QFileInfo(destination).size() != source_size) {
      QFile::remove(destination);
      ++result.failed;
      continue;
    }

    PendingReport item;
    item.uuid = uuid;
    item.created_at = static_cast<qint64>(report.creation_time);
    item.size = source_size;
    exported.push_back(item);
    ++result.exported;

    // 已交付给开发者：转入 completed——不再出现在待处理列表里，但文件不删。
    // 转移失败不当作导出失败（文件已经在导出目录里了），报告留在 pending，
    // 下次启动会再提示一次，宁可多提示也不丢数据
    database->SkipReportUpload(
        report.uuid,
        ::crashpad::Metrics::CrashSkippedReason::kUploadsDisabled);
  }

  if (!exported.isEmpty() &&
      !WriteSummary(target_dir + QStringLiteral("/summary.txt"),
                    BuildSummary(info, exported))) {
    result.error = QStringLiteral("崩溃报告已导出，但 summary.txt 写入失败");
  }

  return result;
}

}  // namespace detail
}  // namespace crash
