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
//    ├─ 翻译（Qt 自带控件 + 功能区）── 必须在任何界面构造之前
//    ├─ 工具模式（--export-report / --crash）─ 无界面，跑完即退出
//    ├─ 上次崩溃的报告提示 ─ 只在有报告时弹，且在登录之前
//    ├─ 功能引导（控制器 + 桥接）─ 必须在登录之前
//    │    外部脚本的第一步常常跑在登录对话框上，那时主窗口还不存在
//    ├─ 登录
//    ├─ 初始化会话 → 主窗口（尺寸由窗口自己从配置恢复）
//    ├─ 主窗口之后接线（引导的 viewport/主题/菜单、单实例激活）
//    └─ app.exec() → 清理
//
// 对象所有权的约定（三种并存是有意的，别随手改）：
//   短生命周期                  → 栈对象（LoginDialog /
//                                 MainWindow）
//   活到进程结束，且是 QObject   → 挂到 app 或宿主窗口下，由 Qt 父子回收
//   活到进程结束，但不是 QObject → unique_ptr（如 TourBridge，它不能有
//                                 父级）
//
// 标了「可删」的段落不需要时整段拿掉即可，不影响其余部分。

#ifdef APP_UI_FLUENT
#include <fluentui3style.h>
#else
#include <style.h>
#endif

#include <tour_bridge.h>
#include <tour_controller.h>
#include <tour_menu.h>

#include <cstdio>
#include <memory>
#include <optional>

#include <QApplication>
#include <QByteArray>
#include <QDialog>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QLibraryInfo>
#include <QString>
#include <QTimer>
#include <QTranslator>
#include <QVector>

#include "AppInfo.h"
#include "AppVersion.h"
#include "auth/auth_service.h"
#include "common/single_instance.h"
#include "config/config_defs.h"
#include "config/config_manager.h"
#include "crashhandler/crash_bootstrap.h"
#include "crashhandler/global_exception_handler.h"
#include "dialogs/crash_report_dialog.h"
#include "dialogs/login_dialog.h"
#include "launch_options.h"
#include "logger/logger.h"
#include "mainwindow.h"
#include "tour_app_topics.h"

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

// 主动崩溃。独立成函数是为了让崩溃栈里出现本函数的帧——演练要验证"符号能
// 恢复出行号"，帧落在 libc 里就什么都验不出来。volatile 保证这次写内存不被
// 优化掉
void TriggerTestCrash() {
  volatile int* null_pointer = nullptr;
  *null_pointer = 42;
}

// crash/backend 的字符串值 → 后端枚举。空串与非法值回落默认（crashpad）：
// 配置写坏不该拦启动
crash::Backend ParseCrashBackend(const QString& value) {
  if (value.isEmpty() || value == QLatin1String(kCrashBackendCrashpad)) {
    return crash::Backend::kCrashpad;
  }
  if (value == QLatin1String(kCrashBackendBuiltin)) {
    return crash::Backend::kBuiltin;
  }
  if (value == QLatin1String(kCrashBackendNone)) {
    return crash::Backend::kNone;
  }
  LOG_WARN("MAIN", "crash/backend 配置非法：{}，回落 crashpad",
           value.toStdString());
  return crash::Backend::kCrashpad;
}

// 应用 --set 注入的配置覆盖。必须在 Logger::init 之前调（log/level 这类键是
// init 时读的，覆盖要赶在它前面），而那时日志还没起——坏条目攒着返回，由
// 调用方等 Logger 起来后统一告警
QStringList ApplyConfigOverrides(const QStringList& raw_overrides) {
  QStringList bad;
  auto& cfg = ConfigManager::instance();
  for (const QString& item : raw_overrides) {
    const qsizetype eq = item.indexOf(QLatin1Char('='));
    const QString key = eq > 0 ? item.left(eq).trimmed() : QString();
    if (key.isEmpty()) {
      bad << item;
      continue;
    }
    cfg.setOverride(key, item.mid(eq + 1));
  }
  return bad;
}

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

// 装载中文翻译。两个 QTranslator 用函数内静态对象持有：installTranslator 只记
// 指针，栈对象出函数就悬空，静态对象活到进程结束、指针始终有效。
//
// 界面语言是写死的：本仓自己的文案全是硬编码中文（规矩是不用 tr()），需要翻
// 译的只有第三方控件的自带串。所以这里不问系统语言，一律 zh_CN——否则中文
// 界面里会混进一段英文。
void InstallTranslations(QApplication& app) {
  // Qt 自带控件的文案（QMessageBox / QFileDialog 的按钮等）。目录问
  // QLibraryInfo：Windows 下是安装目录旁的 translations/（windeployqt
  // 已拷过去），Linux 下是 /usr/share/qt5/translations
  static QTranslator qt_translator;
  static QTranslator saribbon_translator;
  const QString qt_qm = QLibraryInfo::location(QLibraryInfo::TranslationsPath) +
                        QStringLiteral("/qt_zh_CN.qm");
  if (qt_translator.load(qt_qm)) {
    app.installTranslator(&qt_translator);
    LOG_INFO("MAIN", "已装载 Qt 中文翻译: {}", qt_qm.toStdString());
  } else {
    LOG_WARN("MAIN", "Qt 中文翻译装载失败，标准对话框按钮将显示英文: {}",
             qt_qm.toStdString());
  }

  // 功能区（SARibbon）的文案：标题栏按钮的提示、标题图标右键菜单等。.qm 由
  // 构建期从 src/app/translations/SARibbon_zh_CN.ts 编出来并嵌进资源
  // （见 src/app/CMakeLists.txt）。没装 Qt 的 LinguistTools 时它不存在，
  // 这里只降级、不拦启动
  const QString saribbon_qm =
      QStringLiteral(":/translations/SARibbon_zh_CN.qm");
  if (saribbon_translator.load(saribbon_qm)) {
    app.installTranslator(&saribbon_translator);
    LOG_INFO("MAIN", "已装载功能区中文翻译: {}", saribbon_qm.toStdString());
  } else {
    LOG_WARN("MAIN", "功能区中文翻译装载失败，功能区自带文案将显示英文");
  }
}

// 窗口已显示之后的收尾接线：单实例激活。
// 「可删」——不需要单实例时整段拿掉（窗口尺寸的记忆在 MainWindow 里）
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
  QApplication app(argc, argv);
  SetupAppMetadata(app);
  const LaunchOptions opts = ParseCommandLine(QCoreApplication::arguments());
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
  // （它们要把消息写进日志）；崩溃后端配置在两者之间读出
  {
    // 全局样式：两条界面路线互斥（APP_UI_ROUTE，见顶层与
    // src/app/CMakeLists.txt）
    //   fluent —— 换掉整个样式底座（FluentUI3Style 是个 QProxyStyle）。
    //             **不加载任何 QSS**：只要 setStyleSheet 生效，命中规则的控件
    //             就走 QStyleSheetStyle，Fluent 的绘制被整个绕过，结果是界面
    //             一半 Fluent、一半样式表
    //   qss —— ApplyStyleSheet 装两层：先是本仓的底座层（src/style 的
    //          base/*.qss，只写类型/伪状态/子控件/属性选择器），再是本仓业务
    //          控件的项目层（objectName 规则）。底座在前、项目层在后，两层重合
    //          时以项目层为准，顺序固定在 ApplyStyleSheet 里
    // 全局字体两条路都要设（ApplyStyleSheet 内部也会设一次，这里是 fluent
    // 路线也要的那份）
#ifdef APP_UI_FLUENT
    QApplication::setStyle(new FluentUI3Style);
#else
    ApplyStyleSheet(app, {QStringLiteral(":/resources/styles/default.qss")});
#endif
    QFont app_font = app.font();
    app_font.setFamily(QStringLiteral("Microsoft YaHei"));
    app.setFont(app_font);

    ConfigManager::instance();
    // 命令行覆盖（--set key=value）：先于一切业务 get 注入，本次运行内
    // get 全部读到覆盖值，不落盘——外部驱动不必改用户的配置文件
    const QStringList bad_overrides =
        ApplyConfigOverrides(opts.config_overrides);

#ifdef APP_ENABLE_RIBBON
    // 功能区（SARibbon）的图标资源注册。要早于主窗口构造，否则第一帧没有图标
    InitRibbonResources();
#endif

    Logger::init();
    LOG_INFO("MAIN", "应用程序启动，版本: {}", getProjectVersionAll());
    // 这次构建走的是哪条界面路线（两条路的外观差别很大，报出来省得对着截图猜）
#ifdef APP_UI_FLUENT
    LOG_INFO("MAIN", "界面路线：fluent（FluentUI3Style + Frameless 标题栏）");
#elif defined(APP_ENABLE_RIBBON)
    LOG_INFO("MAIN", "界面路线：qss + SARibbon 功能区（底座 + 项目层样式表）");
#else
    LOG_INFO("MAIN", "界面路线：qss（底座 + 项目层样式表）");
#endif

    // --set 的结果汇报（注入在 ConfigManager::instance() 后就做了，那时
    // 日志还没起）
    if (!opts.config_overrides.isEmpty()) {
      LOG_INFO("MAIN", "命令行覆盖配置 {} 项（--set，仅本次运行生效）：{}",
               opts.config_overrides.size(),
               opts.config_overrides.join(QStringLiteral(", ")).toStdString());
      for (const QString& bad : bad_overrides) {
        LOG_WARN("MAIN", "--set 条目非法，已忽略：{}", bad.toStdString());
      }
    }

    // 崩溃捕获后端：运行时配置（crash/backend，重启生效），三个实现都编在
    // 程序里，这里只挑一个装。必须在 GEH 与崩溃捕获初始化之前读出
    const crash::Backend backend =
        ParseCrashBackend(ConfigManager::instance().get<QString>(
            QString::fromLatin1(kConfigCrashBackend),
            QString::fromLatin1(kCrashBackendCrashpad)));

    // 信号捕获 + Qt 消息重定向。崩溃类信号接不接随本次选择的后端：
    //   crashpad —— 必须让出：crashpad_handler 靠进程外机制捕获崩溃，若这里
    //               用 std::signal 抢先接住 SIGSEGV/SIGABRT，handler 收不到
    //               通知，dump 不会生成。只留 SIGINT/SIGTERM
    //   builtin  —— 照常注册：CrashHandler 在本模块之后注册，Windows 侧更是
    //               VEH 先响应，两者不冲突
    //   none     —— 也传 false：本仓不装捕获，崩溃信号留给使用者自己的方案
    GlobalExceptionHandler::instance().init(
        /*enable_crash_signals=*/backend == crash::Backend::kBuiltin);

    // 崩溃捕获：后端由上面的配置决定
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
      const crash::AppInfo app_info{QCoreApplication::applicationName(),
                                    QString::fromLatin1(getProjectVersionAll()),
                                    QString::fromLatin1(getProjectGitCommit()),
                                    QString::fromLatin1(APP_BUILD_TYPE)};

      if (crash::Init(handler_path, app_info, backend)) {
        switch (backend) {
          case crash::Backend::kCrashpad:
            LOG_INFO("MAIN", "崩溃捕获模块初始化完成（Crashpad）");
            break;
          case crash::Backend::kBuiltin:
            LOG_INFO("MAIN", "崩溃捕获模块初始化完成（自研）");
            break;
          case crash::Backend::kNone:
            LOG_INFO("MAIN", "未启用崩溃捕获（crash/backend=none）");
            break;
        }
      } else {
        LOG_WARN("MAIN", "崩溃捕获初始化失败，本次运行不会生成崩溃报告");
      }
    }
  }

  InstallTranslations(app);

  // ========================= 3.6 工具模式与崩溃报告提示 =================
  // 导出崩溃报告：无界面，跑完即退出（售后兜底与自动化演练都走这里）
  if (opts.export_report) {
    return RunCrashReportExport(opts.export_report_dir);
  }

  if (opts.crash_test) {
    LOG_INFO("MAIN", "触发测试崩溃（--crash）");
    TriggerTestCrash();
  }

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
  auto* tour_controller = new TourController(&app);
  auto tour_bridge = std::make_unique<TourBridge>(tour_controller->overlay());

  // ========================= 5. 登录 =========================
  if (opts.ui_test_script.isEmpty() &&
      ConfigManager::instance().get(kConfigLoginEnabled, true)) {
    LoginDialog login;
    if (login.exec() != QDialog::Accepted) {
      LOG_INFO("MAIN", "未登录，退出程序");
      GlobalExceptionHandler::instance().shutdown();
      Logger::shutdown();
      return 0;
    }
    LOG_INFO("MAIN", "登录成功，用户会话已建立,当前用户: {}",
             AuthService::instance().currentUser().user_name_.toStdString());
  } else {
    LOG_INFO("MAIN", "UI 测试回放模式，跳过登录: {}",
             opts.ui_test_script.toStdString());
  }

  // ========================= 主窗口 =========================
  MainWindow w;
  w.show();

  // ========================= 7. 窗口之后的接线 =========================
  // 引导的 viewport 与主题要靠 findChild 找到主窗口里的控件，所以只能等窗口
  // 建好之后再接
  tour_controller->setViewport(&w);
  TourTopic* tour_topic = tour_controller->addTopic(BuildTemplateTourTopic(&w));
  auto* tour_menu = new TourMenu(tour_controller, &w);
  // 主题显式传给菜单：菜单文字写的是「模板导览」，那就得真播模板导览那一条，
  // 不能再靠"第一个注册的"这个位置关系
  tour_menu->attachMenu(&w, tour_topic);

  AttachHostFeatures(single_instance, w);
  SetupUiTesting(w, opts);

  // ========================= 8. 主循环与清理 =========================
  const int ret = app.exec();

  GlobalExceptionHandler::instance().shutdown();
  LOG_INFO("MAIN", "应用程序退出");
  Logger::shutdown();
  return ret;
}
