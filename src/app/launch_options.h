#ifndef FILE_APP_LAUNCH_OPTIONS_H_
#define FILE_APP_LAUNCH_OPTIONS_H_

#include <QString>
#include <QStringList>

// 命令行参数。只有回放与崩溃两族开关；--help / --version 由 QCommandLineParser
// 白送。用 QCommandLineParser 而不是手写扫 arguments()：加参数时能顺带拿到帮助
// 文案与拼写检查，未知参数会直接报错，而不是被静默忽略
struct LaunchOptions {
  // 非空 = UI 测试回放模式（跳过登录，播完即退出，退出码就是测试结果）
  QString ui_test_script;
  // --slow 的原始文本。留着不解析成数字，是因为校验不通过时要报一句人话，
  // 而 ParseCommandLine 跑在 Logger::init() 之前——那里 LOG_WARN 是空转的。
  // 解析与报错都放到 SetupUiTesting 里做（那儿 Logger 已经起来了）
  QString slow_text;
  // --export-report：导出待发送的崩溃报告后退出（无界面）。
  // --export-dir：导出目录，不写则用默认目录（桌面的 CrashReport-<时间戳>/）。
  // 分成两个选项而不是一个"取值可选"的 --export-report [目录]：实测 Qt 只要
  // 给了 valueName，即便默认值是空串，parser.process() 也会报
  // "Missing value after '--export-report'." 并结束进程——"取值可选"在 Qt 里
  // 做不到（这条由 tests/app/launch_options_test.cpp 钉住）
  bool export_report = false;
  QString export_report_dir;
  // --crash：主动触发一次崩溃，用于验证崩溃捕获与回传链路（仅测试用）
  bool crash_test = false;
};

// 解析命令行。入参传 QCoreApplication::arguments()（含程序名 argv[0]，Qt 会跳过
// 第一个元素）。
//
// 单独成文件而不是留在 main.cpp：这是唯一一处纯解析、既不碰界面也不参与启动
// 顺序的逻辑，抽出来之后可以脱离整个应用去测（见 tests/app/launch_options_test.cpp）。
//
// 注意它用的是 QCommandLineParser::process()：--help / --version 会直接打印并
// 结束进程，未知参数也会报错退出——这是刻意的，见上面 struct 的注释。
LaunchOptions ParseCommandLine(const QStringList& arguments);

#endif  // FILE_APP_LAUNCH_OPTIONS_H_
