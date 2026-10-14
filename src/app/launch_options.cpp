#include "launch_options.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

const LaunchOptions& LaunchOptions::instance() {
  static const LaunchOptions opts =
      ParseCommandLine(QCoreApplication::arguments());
  return opts;
}

QString CommandLineText() {
  // 本次运行的实际命令行（argv[0] + 全部实参，原样）。含空格的参数补引号，
  // 贴回终端仍是一条能用的命令
  const QStringList arguments = QCoreApplication::arguments();
  QStringList parts;
  parts.reserve(arguments.size());
  for (const QString& argument : arguments) {
    if (argument.contains(QLatin1Char(' '))) {
      parts << QStringLiteral("\"%1\"").arg(argument);
    } else {
      parts << argument;
    }
  }
  return parts.join(QLatin1Char(' '));
}

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

  const QCommandLineOption doc_option(
      QStringLiteral("doc"), QStringLiteral("启动时打开这份 Markdown 文档"),
      QStringLiteral("文件"));
  parser.addOption(doc_option);

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
  opts.doc_path = parser.value(doc_option);
  opts.config_overrides = parser.values(set_option);
  return opts;
}
