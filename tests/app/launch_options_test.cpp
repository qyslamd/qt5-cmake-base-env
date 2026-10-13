// 命令行解析的单测。它是纯解析层：不需要起界面，也不需要整个应用在场。
//
// 注意 ParseCommandLine 走的是 QCommandLineParser::process()——遇到 --help /
// --version / 未知参数会直接打印并结束进程。所以这里只喂合法命令行，那几个
// 输出到终端的路径由人工与演练覆盖。
#include "launch_options.h"

#include <gtest/gtest.h>
#include <initializer_list>

#include <QString>
#include <QStringList>

namespace {

// 入参形态与 QCoreApplication::arguments() 一致：第一个元素是程序名，
// QCommandLineParser 会跳过（下面 DefaultsAreEmpty 顺带钉住了这件事）
QStringList Args(std::initializer_list<const char*> args) {
  QStringList list;
  for (const char* arg : args) {
    list << QString::fromLatin1(arg);
  }
  return list;
}

}  // namespace

TEST(LaunchOptionsTest, DefaultsAreEmpty) {
  const LaunchOptions opts = ParseCommandLine(Args({"demo_app"}));
  EXPECT_TRUE(opts.ui_test_script.isEmpty());
  EXPECT_TRUE(opts.slow_text.isEmpty());
  EXPECT_FALSE(opts.export_report);
  EXPECT_TRUE(opts.export_report_dir.isEmpty());
  EXPECT_TRUE(opts.config_overrides.isEmpty());
}

// --set 可重复，values() 按出现顺序收全。原始 "key=value" 串原样保留——
// 拆解与容错在 main 的 ApplyConfigOverrides 里做
TEST(LaunchOptionsTest, SetOverridesCollectedInOrder) {
  const LaunchOptions opts = ParseCommandLine(
      Args({"demo_app", "--set", "login/enabled=false", "--set",
            "crash/backend=none", "--play", "smoke.xml"}));
  ASSERT_EQ(opts.config_overrides.size(), 2);
  EXPECT_EQ(opts.config_overrides.at(0), QStringLiteral("login/enabled=false"));
  EXPECT_EQ(opts.config_overrides.at(1), QStringLiteral("crash/backend=none"));
  EXPECT_EQ(opts.ui_test_script, QStringLiteral("smoke.xml"));
}

TEST(LaunchOptionsTest, PlayAndSlow) {
  const LaunchOptions opts = ParseCommandLine(
      Args({"demo_app", "--play", "smoke.xml", "--slow", "200"}));
  EXPECT_EQ(opts.ui_test_script, QStringLiteral("smoke.xml"));
  EXPECT_EQ(opts.slow_text, QStringLiteral("200"));
}

// 不带 --export-dir 时目录为空串，main 据此退回默认目录（桌面的
// CrashReport-<时间戳>/）。两个字段一个都不能少：export_report 决定"是否导出"，
// 目录决定"导到哪"
TEST(LaunchOptionsTest, ExportReportWithoutDirectory) {
  const LaunchOptions opts =
      ParseCommandLine(Args({"demo_app", "--export-report"}));
  EXPECT_TRUE(opts.export_report);
  EXPECT_TRUE(opts.export_report_dir.isEmpty());
}

TEST(LaunchOptionsTest, ExportReportWithDirectory) {
  const LaunchOptions opts = ParseCommandLine(
      Args({"demo_app", "--export-report", "--export-dir", "/tmp/crash-out"}));
  EXPECT_TRUE(opts.export_report);
  EXPECT_EQ(opts.export_report_dir, QStringLiteral("/tmp/crash-out"));
}

// 只给 --export-dir 不构成导出：选项是开关，目录只是它的参数
TEST(LaunchOptionsTest, ExportDirAloneDoesNotEnableExport) {
  const LaunchOptions opts =
      ParseCommandLine(Args({"demo_app", "--export-dir", "/tmp/crash-out"}));
  EXPECT_FALSE(opts.export_report);
  EXPECT_EQ(opts.export_report_dir, QStringLiteral("/tmp/crash-out"));
}

// 这条是踩过的坑：曾有实现把 --export-report
// 做成"取值可选"（靠给默认值传空串）， 实测 Qt 只要给了 valueName 就会报
// "Missing value after ..." 并结束进程。改成 开关 + --export-dir
// 之后，带值的选项都由 Qt 校验，两种写法也都支持
TEST(LaunchOptionsTest, ExportDirAcceptsEqualsForm) {
  const LaunchOptions opts =
      ParseCommandLine(Args({"demo_app", "--export-dir=/tmp/crash-out"}));
  EXPECT_EQ(opts.export_report_dir, QStringLiteral("/tmp/crash-out"));
}
