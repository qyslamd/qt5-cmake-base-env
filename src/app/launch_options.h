#ifndef FILE_APP_LAUNCH_OPTIONS_H_
#define FILE_APP_LAUNCH_OPTIONS_H_

#include <QString>
#include <QStringList>

// 命令行参数，整个应用的全局上下文：任何模块经 LaunchOptions::instance()
// 取，不必把结构体一层层传下去。只有回放与导出两族开关；--help / --version
// 由 QCommandLineParser 白送。用 QCommandLineParser 而不是手写扫 arguments()：
// 加参数时能顺带拿到帮助文案与拼写检查，未知参数会直接报错，而不是被静默忽略
struct LaunchOptions {
  // 全局访问点：首次访问时解析进程命令行并缓存。main 之外要读命令行走这里
  static const LaunchOptions& instance();

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
  // --doc <文件>：启动即打开这份 Markdown 文档。给脚本与 funq 探针一个不点
  // 菜单就能开文档的入口
  QString doc_path;
  // --set key=value 的原始条目（可重复）。留原始串的原因同 slow_text：解析
  // 与告警要等 Logger::init 之后才有日志可用，放到 main 里做。
  // 语义：同名键覆盖 INI 与注册默认值，仅本次运行生效、不落盘——外部驱动
  // （funq 客户端 / AI agent 脚本）不必碰用户的配置文件就能换行为，
  // 例如 --set login/enabled=false --set crash/backend=none
  QStringList config_overrides;
};

// 解析命令行。入参传 QCoreApplication::arguments()（含程序名 argv[0]，Qt 会跳过
// 第一个元素）。应用代码一般不走它，直接用 instance()；它是纯函数，供单测
// 喂任意命令行（见 tests/app/launch_options_test.cpp）。
//
// 注意它用的是 QCommandLineParser::process()：--help / --version 会直接打印并
// 结束进程，未知参数也会报错退出——这是刻意的，见上面 struct 的注释。
LaunchOptions ParseCommandLine(const QStringList& arguments);

// 本次运行的实际命令行（argv[0] + 全部实参，原样），供界面展示与日志记录。
// 与"支持哪些参数"无关——那是 -h/--help 的事
QString CommandLineText();

#endif  // FILE_APP_LAUNCH_OPTIONS_H_
