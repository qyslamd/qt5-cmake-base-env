#include "launch_options.h"

#include <QCommandLineOption>
#include <QCommandLineParser>

LaunchOptions ParseCommandLine(const QStringList& arguments) {
  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Qt/CMake 工程模板：分层结构，自带日志、配置、单实例、"
                     "崩溃捕获与注入式 UI 测试"));
  parser.addHelpOption();
  parser.addVersionOption();

  const QCommandLineOption play_option(
      QStringLiteral("play"),
      QStringLiteral("回放 UI 测试脚本后退出（需要 BUILD_QT_TESTING 的构建）"),
      QStringLiteral("脚本"));
  parser.addOption(play_option);

  const QCommandLineOption slow_option(
      QStringLiteral("slow"),
      QStringLiteral("回放时每条事件之间等待的毫秒数。只对人看着回放有用，"
                     "给 CI 跑不要加"),
      QStringLiteral("毫秒"));
  parser.addOption(slow_option);

  // --export-report 是开关，目录另用 --export-dir：Qt 做不到"取值可选"
  // （见 launch_options.h 里的说明），拆成两个选项才能既保留"不指定目录就用
  // 桌面"的行为，又不必让人写 --export-report desktop 这种假值
  const QCommandLineOption export_option(
      QStringLiteral("export-report"),
      QStringLiteral("导出待发送的崩溃报告后退出（无界面）"));
  parser.addOption(export_option);

  const QCommandLineOption export_dir_option(
      QStringLiteral("export-dir"),
      QStringLiteral("导出到指定目录；不写则导出到桌面的 "
                     "CrashReport-<时间戳>/"),
      QStringLiteral("目录"));
  parser.addOption(export_dir_option);

  // 售后演练用：客户机器上也能走一遍"崩溃 → 捕获 → 导出"整条链路
  const QCommandLineOption crash_option(
      QStringLiteral("crash"),
      QStringLiteral("主动触发一次崩溃（用于验证崩溃捕获与回传链路）"));
  parser.addOption(crash_option);

  // 可重复的配置覆盖（见 launch_options.h 的说明）。values() 收集全部出现
  const QCommandLineOption set_option(
      QStringLiteral("set"),
      QStringLiteral("覆盖一项配置（key=value，可重复），仅本次运行生效、"
                     "不写入配置文件。例：--set login/enabled=false "
                     "--set crash/backend=none"),
      QStringLiteral("key=value"));
  parser.addOption(set_option);

  parser.process(arguments);

  LaunchOptions opts;
  opts.ui_test_script = parser.value(play_option);

  opts.slow_text = parser.value(slow_option);

  opts.export_report = parser.isSet(export_option);
  opts.export_report_dir = parser.value(export_dir_option);
  opts.config_overrides = parser.values(set_option);
  opts.crash_test = parser.isSet(crash_option);
  return opts;
}
