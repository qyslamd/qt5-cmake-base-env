// 崩溃报告导出：不依赖真崩溃，也不碰开发者本机真实的崩溃库。
//
// 用临时目录 + crashpad 的正规入口造一份 pending 报告（PrepareNewCrashReport
// 写进 new/，FinishedWritingCrashReport 搬进 pending/），然后走
// crash::ExportPendingReports 的纯函数形态，验证落盘内容与"交付后不再提示、
// 但文件不删"的语义。
#include <cstring>
#include <memory>

#include <gtest/gtest.h>

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVector>

#include "client/crash_report_database.h"
#include "crashhandler/crash_bootstrap.h"

namespace {

// QString → base::FilePath：Windows 下用 wstring，POSIX 下用 string。
// 与 src/core/crashhandler/crashpad_bootstrap.cpp 里的同名辅助一致
base::FilePath ToFilePath(const QString& s) {
#ifdef _WIN32
  return base::FilePath(s.toStdWString());
#else
  return base::FilePath(s.toStdString());
#endif
}

// 造一份 pending 报告，返回它的 uuid
bool MakePendingReport(const QString& db_dir, QString* uuid) {
  using Database = crashpad::CrashReportDatabase;

  std::unique_ptr<Database> database = Database::Initialize(ToFilePath(db_dir));
  if (!database) {
    return false;
  }

  std::unique_ptr<Database::NewReport> report;
  if (database->PrepareNewCrashReport(&report) != Database::kNoError) {
    return false;
  }

  // 内容是什么无所谓——这里验的是搬运与落盘，不是 dump 的可解析性
  const char payload[] = "fake minidump payload";
  if (!report->Writer()->Write(payload, sizeof(payload))) {
    return false;
  }

  crashpad::UUID report_uuid;
  if (database->FinishedWritingCrashReport(std::move(report), &report_uuid) !=
      Database::kNoError) {
    return false;
  }

  *uuid = QString::fromStdString(report_uuid.ToString());
  return true;
}

QString ReadAll(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return QString();
  }
  QTextStream in(&file);
  in.setCodec("UTF-8");
  return in.readAll();
}

// 库里是否还存在这个文件。交付后的目录名随平台不同——Linux 走通用实现是
// completed/，Windows 是 reports/——所以按"还在库里某处"来断言，不钉死路径
bool ExistsUnder(const QString& dir, const QString& file_name) {
  QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    if (QFileInfo(it.next()).fileName() == file_name) {
      return true;
    }
  }
  return false;
}

crash::AppInfo TestAppInfo() {
  crash::AppInfo info;
  info.name = QStringLiteral("crash_report_test");
  info.version = QStringLiteral("V9.9.9.9");
  info.git_commit = QStringLiteral("deadbee-dirty");
  info.build_config = QStringLiteral("RelWithDebInfo");
  return info;
}

}  // namespace

TEST(CrashReportTest, ListAndExportPendingReports) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  const QString db_dir = temp.path() + QStringLiteral("/crash");
  const QString target_dir = temp.path() + QStringLiteral("/out");

  QString uuid;
  ASSERT_TRUE(MakePendingReport(db_dir, &uuid)) << "造 pending 报告失败";

  const QVector<crash::PendingReport> pending =
      crash::ListPendingReports(db_dir);
  ASSERT_EQ(pending.size(), 1);
  EXPECT_EQ(pending.first().uuid, uuid);
  EXPECT_GT(pending.first().size, 0);

  const crash::ExportResult result =
      crash::ExportPendingReports(db_dir, target_dir, TestAppInfo());
  ASSERT_TRUE(result.ok()) << result.error.toStdString();
  EXPECT_EQ(result.exported, 1);
  EXPECT_EQ(result.failed, 0);
  EXPECT_EQ(result.target_dir, target_dir);

  const QString dumped =
      target_dir + QLatin1Char('/') + uuid + QStringLiteral(".dmp");
  ASSERT_TRUE(QFile::exists(dumped));
  EXPECT_EQ(QFileInfo(dumped).size(), pending.first().size);

  // summary.txt 是开发者在打开 dump 之前先看的东西：版本、git 提交、构建配置
  // 得能自证"这份崩溃来自哪个构建"
  const QString summary = ReadAll(target_dir + QStringLiteral("/summary.txt"));
  const crash::AppInfo info = TestAppInfo();
  EXPECT_NE(summary.indexOf(info.version), -1);
  EXPECT_NE(summary.indexOf(info.git_commit), -1);
  EXPECT_NE(summary.indexOf(info.build_config), -1);
  EXPECT_NE(summary.indexOf(uuid), -1);

  // 交付语义：报告离开 pending 所以不再提示，但文件还在——客户把导出目录
  // 弄丢了也还有救。文件不在 pending 里、又还在库里，就是"被转走而不是被删"
  EXPECT_TRUE(crash::ListPendingReports(db_dir).isEmpty());
  EXPECT_TRUE(ExistsUnder(db_dir, uuid + QStringLiteral(".dmp")));
}

TEST(CrashReportTest, NoReportsIsNotAnError) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  const QString db_dir = temp.path() + QStringLiteral("/crash");

  EXPECT_TRUE(crash::ListPendingReports(db_dir).isEmpty());

  const crash::ExportResult result = crash::ExportPendingReports(
      db_dir, temp.path() + QStringLiteral("/out"), TestAppInfo());
  EXPECT_TRUE(result.ok()) << result.error.toStdString();
  EXPECT_EQ(result.exported, 0);
  EXPECT_EQ(result.failed, 0);
}
