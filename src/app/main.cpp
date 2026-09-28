// 应用入口。
//
// 启动顺序（这个顺序本身有约束，改之前先看对应那段的注释）：
//
//   QApplication
//    ├─ 应用元信息（组织名 / 应用名 / 图标 / 版本）
//    │    决定数据目录、单实例键与日志文件名，须与安装脚本的卸载路径一致
//    ├─ 解析命令行
//    ├─ 单实例检测 ─────────── 必须在 Logger 之前（拒绝回放时只能写 stderr）
//    ├─ 样式 → ConfigManager → Logger → GEH → 崩溃捕获
//    │    严格顺序，见各步注释
//    ├─ 工具模式（--export-report / --crash）─ 无界面，跑完即退出
//    ├─ 上次崩溃的报告提示 ─ 只在有报告时弹，且在登录之前
//    ├─ 功能引导（控制器 + 桥接）─ 必须在登录之前
//    │    外部脚本的第一步常常跑在登录对话框上，那时主窗口还不存在
//    ├─ 登录
//    ├─ Splash：会话 → 配置 → 主窗口
//    ├─ 主窗口之后接线（引导的 viewport/主题/菜单、单实例激活、退出保存）
//    └─ app.exec() → 清理
//
// 对象所有权的约定（三种并存是有意的，别随手改）：
//   短生命周期                  → 栈对象（LoginDialog / SplashScreen /
//                                 MainWindow）
//   活到进程结束，且是 QObject   → 挂到 app 或宿主窗口下，由 Qt 父子回收
//   活到进程结束，但不是 QObject → unique_ptr（如 GuidanceBridge，它不能有
//                                 父级）
//
// 标了「可删」的段落不需要时整段拿掉即可，不影响其余部分。

#include <guidance_bridge.h>
#include <guidance_controller.h>
#include <guidance_menu.h>
#include <style.h>

#include <cstdio>
#include <memory>
#include <optional>

#include <QApplication>
#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDialog>
#include <QIcon>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <qt-libui/splash_screen.h>

#include "AppInfo.h"
#include "AppVersion.h"
#include "auth/auth_service.h"
#include "common/global_exception_handler.h"
#include "common/single_instance.h"
#include "config/config_defs.h"
#include "config/config_manager.h"
#include "crashhandler/crash_bootstrap.h"
#include "dialogs/crash_report_dialog.h"
#include "dialogs/login_dialog.h"
#include "guidance_app_flows.h"
#include "logger/logger.h"
#include "mainwindow.h"

#ifdef APP_ENABLE_QT_TESTING
#include <pqEventDispatcher.h>

#include "qttesting/testing_support.h"
#endif

#ifdef APP_ENABLE_RIBBON
// SARibbon 的图标编在静态库自己的 qrc 里，没人引用那个符号就会被链接器丢掉，
// 表现为功能区上的按钮全是空白（本仓在 QtTesting 的面板上踩过同款）。
//
// 这个函数必须留在文件作用域：Q_INIT_RESOURCE 展开是一条 extern 声明，
// 放进匿名命名空间会绑到 internal linkage 的符号上，链接期直接报找不到
// qInitResources_SARibbonResource
static void InitRibbonResources() { Q_INIT_RESOURCE(SARibbonResource); }
#endif

namespace {

// 命令行参数。只有回放一个开关；--help / --version 由 QCommandLineParser 白送。
// 用 QCommandLineParser 而不是手写扫 arguments()：加参数时能顺带拿到帮助文案与
// 拼写检查，未知参数会直接报错，而不是被静默忽略
struct LaunchOptions {
  // 非空 = UI 测试回放模式（跳过登录，播完即退出，退出码就是测试结果）
  QString ui_test_script;
  // --slow 的原始文本。留着不解析成数字，是因为校验不通过时要报一句人话，
  // 而 ParseCommandLine 跑在 Logger::init() 之前——那里 LOG_WARN 是空转的。
  // 解析与报错都放到 SetupUiTesting 里做（那儿 Logger 已经起来了）
  QString slow_text;
  // --export-report：导出待发送的崩溃报告后退出。值为导出目录，空串表示用
  // 默认目录（桌面）。分成两个字段是为了区分"没给这个选项"与"给了但没写目录"
  bool export_report = false;
  QString export_report_dir;
  // --crash：主动触发一次崩溃，用于验证崩溃捕获与回传链路（仅测试用）
  bool crash_test = false;
};

LaunchOptions ParseCommandLine(QApplication& app) {
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

  // 默认值是"空但非 null"的字符串：Qt 只在这种情况下才允许选项省略取值，
  // 于是 --export-report 与 --export-report <目录> 两种写法都成立
  const QCommandLineOption export_option(
      QStringLiteral("export-report"),
      QStringLiteral("导出待发送的崩溃报告到指定目录后退出"
                     "（省略目录则用桌面），无界面"),
      QStringLiteral("目录"), QStringLiteral(""));
  parser.addOption(export_option);

#ifdef APP_ENABLE_CRASH_TEST
  // 只在开启崩溃测试的构建里存在：发布包不该带一个能让程序主动崩掉的开关
  const QCommandLineOption crash_option(
      QStringLiteral("crash"),
      QStringLiteral("主动触发一次崩溃（用于验证崩溃捕获与回传链路）"));
  parser.addOption(crash_option);
#endif

  parser.process(app);

  LaunchOptions opts;
  opts.ui_test_script = parser.value(play_option);

  opts.slow_text = parser.value(slow_option);

  opts.export_report = parser.isSet(export_option);
  opts.export_report_dir = parser.value(export_option);
#ifdef APP_ENABLE_CRASH_TEST
  opts.crash_test = parser.isSet(crash_option);
#endif
  return opts;
}

void SetupAppMetadata(QApplication& app) {
  // 组织名/应用名来自 CMake，决定 %LOCALAPPDATA% 数据目录、单实例键与
  // 日志文件名，须与安装脚本的卸载路径保持一致
  app.setOrganizationName(QString::fromLatin1(APP_ORGANIZATION_NAME));
  app.setApplicationName(QString::fromLatin1(APP_APPLICATION_NAME));
  app.setWindowIcon(QIcon(":/resources/icons/icon.ico"));
  app.setApplicationVersion(getProjectVersionAll());
}

// 命令行导出崩溃报告：售后兜底（对话框不可用时让客户自己跑一次）与自动化
// 演练都走这里。返回进程退出码：0 = 成功（含"本来就没有报告"），1 = 失败
int RunCrashReportExport(const QString& target_dir) {
  const QString dir =
      target_dir.isEmpty() ? DefaultCrashReportDir() : target_dir;
  const crash::ExportResult result = crash::ExportPendingReports(
      crash::DatabaseDir(), dir, crash::CurrentAppInfo());
  if (!result.ok()) {
    LOG_WARN("MAIN", "导出崩溃报告失败: {}", result.error.toStdString());
    return 1;
  }
  LOG_INFO("MAIN", "导出崩溃报告 {} 份到 {}", result.exported,
           result.target_dir.toStdString());
  return 0;
}

// 有待处理的崩溃报告就提示用户导出。无桌面环境时只记日志：模态框在那儿没人
// 会点，会一直挂到被外力杀掉
void MaybePromptCrashReport() {
  if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
    LOG_INFO("MAIN",
             "无桌面环境，跳过崩溃报告提示（可用 --export-report 导出）");
    return;
  }

  const QVector<crash::PendingReport> reports =
      crash::ListPendingReports(crash::DatabaseDir());
  if (reports.isEmpty()) {
    return;
  }
  LOG_INFO("MAIN", "检测到 {} 份待导出的崩溃报告", reports.size());

  CrashReportDialog dialog(reports);
  dialog.exec();
}

#ifdef APP_ENABLE_CRASH_TEST
// 主动崩溃。独立成函数是为了让崩溃栈里出现本函数的帧——演练要验证"符号能
// 恢复出行号"，帧落在 libc 里就什么都验不出来。volatile 保证这次写内存不被
// 优化掉
void TriggerTestCrash() {
  volatile int* null_pointer = nullptr;
  *null_pointer = 42;
}
#endif

// 已有实例时的处理。返回非空表示应当直接退出，值就是退出码
std::optional<int> HandleAlreadyRunning(SingleInstance& instance,
                                        const LaunchOptions& opts) {
  // 命令行工具模式（导出报告 / 测试崩溃）不参与单实例：
  //   转发给首实例只会让对方闪一下窗口，不落地任何动作，调用方看到的是假成功；
  //   而"因为已经有实例在跑所以退出"同样让脚本拿到假结果。这两种模式都不需要
  //   界面，自己跑完自己的活即可
  if (opts.export_report || opts.crash_test) {
    return std::nullopt;
  }

  if (!instance.isAppAlreadyRunning()) {
    return std::nullopt;
  }

  if (opts.ui_test_script.isEmpty()) {
    // 普通启动的第二实例：把命令行转交给首实例，自己退出
    instance.connectToExistingInstance(QCoreApplication::arguments());
    return 0;
  }

  // 回放不豁免单实例：程序本体只允许一个实例，测试也不例外。
  //
  // 这里必须"明确失败"，两条路都不能走：
  //   转发后以 0 退出 —— ctest 看到"通过"，而脚本一个事件都没跑
  //   跳过检测起第二个 —— 两个实例抢同一份日志与配置（日志那份已经炸过）
  //
  // 退出码 2 与"回放失败"（1）分开，便于一眼看出是环境问题而非脚本问题。
  //
  // 此刻 Logger 还没 init，只能直接写 stderr。不能用 qWarning()：Qt 在
  // Windows 上默认不往管道写（它怕死锁），而 ctest 与各种重定向场景恰恰
  // 就是管道——消息会静默丢掉。
  //
  // 走 QStringLiteral 再转 UTF-8，而不是直接把中文写进 fprintf 的窄字面量：
  // 后者会让 MSVC 报 C4819（窄字面量要落到执行字符集上），而前者是 UTF-16
  const QByteArray message =
      QStringLiteral("已有实例在运行，UI 测试回放要求独占：请先关闭程序再跑。")
          .toUtf8();
  std::fprintf(stderr, "%s\n", message.constData());
  return 2;
}

// Splash 第 2 步的实事：把当前用户会话准备好。
// 单独成函数是为了让 Splash 那段只负责"报进度"，一眼分得清哪条线是进度、
// 哪条线是干活——模板使用者替换时照着改这条线即可
void InitializeSession() {
  const User current_user = AuthService::instance().currentUser();
  LOG_INFO("MAIN", "用户会话已建立: {}", current_user.user_name_.toStdString());
}

// Splash 第 3 步的实事：读回上次的窗口尺寸，并钳制，防止损坏的 INI
// 产生不可用窗口
QSize LoadWindowSize() {
  const int saved_width = ConfigManager::instance().get<int>(
      QString::fromLatin1(kConfigWindowWidth), kConfigWindowDefaultWidth);
  const int saved_height = ConfigManager::instance().get<int>(
      QString::fromLatin1(kConfigWindowHeight), kConfigWindowDefaultHeight);
  return QSize(qBound(640, saved_width, 4096), qBound(480, saved_height, 2160));
}

void SaveWindowSize(const QWidget& window) {
  auto& cfg = ConfigManager::instance();
  cfg.set<int>(QString::fromLatin1(kConfigWindowWidth), window.width());
  cfg.set<int>(QString::fromLatin1(kConfigWindowHeight), window.height());
  cfg.sync();
}

// 窗口已显示之后的收尾接线：单实例激活 + 退出前落盘。
// 「可删」——不需要单实例或不需要记住窗口尺寸时整段拿掉
void AttachHostFeatures(SingleInstance& instance, MainWindow& window) {
  // 第二实例请求激活：Windows 在 SingleInstance 内走 HWND 置前，
  // 其余平台依赖本信号完成 Qt 层的显示与激活
  instance.setActivationWindow(reinterpret_cast<void*>(window.winId()));
  QObject::connect(&instance, &SingleInstance::showApplication, &window,
                   [&window]() {
                     window.showNormal();
                     window.raise();
                     window.activateWindow();
                   });

  // 退出前保存窗口尺寸并立即落盘
  QObject::connect(qApp, &QCoreApplication::aboutToQuit,
                   [&window]() { SaveWindowSize(window); });
}

// UI 测试的接线（录制/回放菜单，或直接回放）。
// 「可删」——不需要录制回放时整段拿掉
void SetupUiTesting(MainWindow& window, const LaunchOptions& opts) {
#ifdef APP_ENABLE_QT_TESTING
  // --slow：放慢回放，方便人看着它一步步走。
  //
  // 用 setEventPlaybackDelay 而不是往脚本里插 pause 事件：pause 是 sleep，
  // 等待期间不处理事件、界面会冻住（库头文件原话 "the application will appear
  // frozen"）；这个 setter 走的是 processEventsAndWait，界面照常刷新。
  // 详见 docs/guide/模板使用指南.md 的「放慢回放」
  if (!opts.slow_text.isEmpty()) {
    bool ok = false;
    const int slow_ms = opts.slow_text.toInt(&ok);
    if (!ok || slow_ms <= 0) {
      LOG_WARN("MAIN", "--slow 需要一个正整数毫秒数，已忽略: {}",
               opts.slow_text.toStdString());
    } else {
      pqEventDispatcher::setEventPlaybackDelay(slow_ms);
      LOG_INFO("MAIN", "回放节流：每条事件之间等 {} ms", slow_ms);
    }
  }

  // 该对象随主窗口一起析构；回放模式下不挂菜单（没人点），改为立即回放
  auto* ui_test = new UiTestSupport(&window);
  if (opts.ui_test_script.isEmpty()) {
    // BUILD_QT_TESTING 打开的构建带「测试 → 录制/回放」菜单
    ui_test->attachMenu(&window);
    return;
  }
  // 窗口已显示再回放，事件才有落点；播完即退出，退出码就是测试结果
  QTimer::singleShot(0, &window, [ui_test, script = opts.ui_test_script]() {
    const bool played = ui_test->playFile(script);
    QCoreApplication::exit(played ? 0 : 1);
  });
#else
  if (!opts.ui_test_script.isEmpty()) {
    LOG_WARN("MAIN", "--play 需要 BUILD_QT_TESTING=ON 的构建，已忽略: {}",
             opts.ui_test_script.toStdString());
  }
  if (!opts.slow_text.isEmpty()) {
    LOG_WARN("MAIN", "--slow 只在 BUILD_QT_TESTING=ON 的构建里有效，已忽略");
  }
#endif
}

}  // namespace

int main(int argc, char* argv[]) {
  // ========================= 1. 应用与参数 =========================
  QApplication app(argc, argv);
  SetupAppMetadata(app);
  const LaunchOptions opts = ParseCommandLine(app);

  // ========================= 2. 单实例（可删）=========================
  SingleInstance single_instance(QCoreApplication::applicationName(),
                                 QString::fromLatin1(APP_MUTEX_NAME));
  if (const auto exit_code = HandleAlreadyRunning(single_instance, opts)) {
    return *exit_code;
  }
  // 工具模式不启动 IPC 服务：首实例可能已经在监听同一个服务名，去抢只会互相
  // 干扰，而这两种模式本来也不需要被激活
  if (!opts.export_report && !opts.crash_test) {
    single_instance.startListening();
  }

  // ========================= 3. 基础设施 =========================
  // 顺序有约束：配置要先于日志（日志读配置），异常/崩溃捕获要晚于日志
  // （它们要把消息写进日志）。
  //
  // 这几步刻意不抽成函数：crash_handler 一旦离开作用域就会析构、顺带撤掉
  // 崩溃捕获，抽出去反而埋雷
  {
    // 全局样式：底座层由 demo_style 提供（顺序与拼接在那边），项目层是本仓
    // 业务控件的 objectName 规则，跟在底座层之后加载。顺带设了全局字体
    ApplyStyleSheet(app, {QStringLiteral(":/resources/styles/default.qss")});

    ConfigManager::instance();

#ifdef APP_ENABLE_RIBBON
    // 功能区（SARibbon）的图标资源注册。要早于主窗口构造，否则第一帧没有图标
    InitRibbonResources();
#endif

    Logger::init();
    LOG_INFO("MAIN", "应用程序启动，版本: {}", getProjectVersionAll());

    // 信号捕获 + Qt 消息重定向。
    //
    // Crashpad 模式下必须传 false：crashpad_handler 通过进程外机制捕获崩溃，
    // 若 GlobalExceptionHandler 用 std::signal 抢先接住 SIGSEGV/SIGABRT，
    // handler 收不到通知，dump 不会生成。此时这里只保留 SIGINT/SIGTERM。
    //
    // 自研模式下传 true：WindowsCrashHandler 的 VEH 会先接住异常并
    // TerminateProcess，走不到 CRT 信号这层，两者不冲突
#ifdef APP_ENABLE_CRASHPAD
    GlobalExceptionHandler::instance().init(/*enable_crash_signals=*/false);
#else
    GlobalExceptionHandler::instance().init();
#endif

    // 崩溃捕获：两套实现由 APP_ENABLE_CRASHPAD 编译期切换
    //   OFF → 自研（信号 / SEH + .log + .dmp，进程内解析符号）
    //   ON  → Crashpad（crashpad_handler 独立进程 + minidump）
    //
    // handler 生命周期由 crash::Init 内部维护（文件作用域静态对象），
    // 不需要在这里持有；数据库目录也由内部按 QStandardPaths 解析
    {
      const QString exe_dir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
      const QString handler_path =
          exe_dir + QStringLiteral("/crashpad_handler.exe");
#else
      const QString handler_path =
          exe_dir + QStringLiteral("/crashpad_handler");
#endif

      // 应用元信息进 dump 的注解，也进导出的 summary.txt：崩溃报告得能自证
      // "我是哪个构建"，才能和 dist/symbols/<版本>-<git hash>-<架构>/ 对上
      const crash::AppInfo app_info{
          QCoreApplication::applicationName(),
          QString::fromLatin1(getProjectVersionAll()),
          QString::fromLatin1(getProjectGitCommit()),
          QString::fromLatin1(APP_BUILD_TYPE)};

      if (crash::Init(handler_path, app_info)) {
        LOG_INFO("MAIN", "崩溃捕获模块初始化完成");
      } else {
        LOG_WARN("MAIN", "崩溃捕获初始化失败，本次运行不会生成崩溃报告");
      }
    }
  }

  // ========================= 3.5 工具模式与崩溃报告提示 =================
  // 导出崩溃报告：无界面，跑完即退出（售后兜底与自动化演练都走这里）
  if (opts.export_report) {
    return RunCrashReportExport(opts.export_report_dir);
  }

#ifdef APP_ENABLE_CRASH_TEST
  if (opts.crash_test) {
    LOG_INFO("MAIN", "触发测试崩溃（--crash）");
    TriggerTestCrash();
  }
#endif

  // 上次崩溃的报告：客户不会自己去 %LOCALAPPDATA% 里翻，这里主动提示导出。
  // 崩溃那一刻进程正在死、没有提示的机会，所以只能等下次启动
  if (!opts.export_report && !opts.crash_test &&
      opts.ui_test_script.isEmpty()) {
    MaybePromptCrashReport();
  }

  // ========================= 4. 功能引导（可删）=========================
  // 控制器驱动演示层（遮罩 + 高亮 + 气泡），桥接对象供外部脚本从进程外驱动。
  //
  // **必须在登录之前**：外部脚本的第一步就跑在登录对话框上，那时主窗口还不
  // 存在，挂到主窗口下的话那几步没法高亮。桥接对象因此是独立顶层窗口，没有
  // Qt 父级，由这里的 unique_ptr 持有（控制器挂在 app 下）
  auto* guidance_controller = new GuidanceController(&app);
  auto guidance_bridge =
      std::make_unique<GuidanceBridge>(guidance_controller->presentation());

  // ========================= 5. 登录 =========================
  // 登录成功前不创建主窗口。回放模式跳过登录：录制入口在主窗口菜单里，
  // 也就是录制总是从「已登录」状态开始；回放只有从同一状态起步，脚本里
  // 记录的控件才存在
  if (opts.ui_test_script.isEmpty()) {
    LoginDialog login;
    if (login.exec() != QDialog::Accepted) {
      LOG_INFO("MAIN", "未登录，退出程序");
      GlobalExceptionHandler::instance().shutdown();
      Logger::shutdown();
      return 0;
    }
    LOG_INFO("MAIN", "登录成功，当前用户: {}",
             AuthService::instance().currentUser().user_name_.toStdString());
  } else {
    LOG_INFO("MAIN", "UI 测试回放模式，跳过登录: {}",
             opts.ui_test_script.toStdString());
  }

  // ========================= 6. Splash 与主窗口 =========================
  // Splash 只负责报进度，干活的是中间那几个具名函数——模板使用者把耗时初始化
  // 插在步骤之间即可，Splash 不需要知道主窗口内部结构
  SplashScreen splash;
  // logo 由消费方提供（库不携带资源），用应用图标
  splash.setLogo(QIcon(":/resources/icons/icon.ico"));
  splash.show();
  QCoreApplication::processEvents();

  splash.setStatusText(QStringLiteral("初始化用户会话"));
  splash.setProgress(25);
  QCoreApplication::processEvents();
  InitializeSession();

  splash.setStatusText(QStringLiteral("加载用户配置"));
  splash.setProgress(50);
  QCoreApplication::processEvents();
  const QSize window_size = LoadWindowSize();

  splash.setStatusText(QStringLiteral("创建主界面"));
  splash.setProgress(80);
  QCoreApplication::processEvents();
  MainWindow w;
  w.resize(window_size);

  splash.setStatusText(QStringLiteral("启动完成"));
  splash.setProgress(100);
  QCoreApplication::processEvents();
  splash.finish();

  w.show();

  // ========================= 7. 窗口之后的接线 =========================
  // 引导的 viewport 与主题要靠 findChild 找到主窗口里的控件，所以只能等窗口
  // 建好之后再接
  guidance_controller->setViewport(&w);
  guidance_controller->addFlow(BuildTemplateTourFlow(&w));
  auto* guidance_menu = new GuidanceMenu(guidance_controller, &w);
  guidance_menu->attachMenu(&w);

  AttachHostFeatures(single_instance, w);
  SetupUiTesting(w, opts);

  // ========================= 8. 主循环与清理 =========================
  const int ret = app.exec();

  GlobalExceptionHandler::instance().shutdown();
  LOG_INFO("MAIN", "应用程序退出");
  Logger::shutdown();
  return ret;
}
