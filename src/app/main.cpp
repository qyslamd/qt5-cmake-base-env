// 应用入口。启动顺序有约束：单实例先于 Logger（拒绝回放时只能写 stderr）、
// 翻译先于任何界面构造、功能引导先于登录（外部脚本第一步常跑在登录框上）。
//
// 所有权三种并存：短生命周期用栈对象；活到进程结束的 QObject 挂 Qt 父子；
// 活到进程结束的非 QObject 用 unique_ptr（如不能有父级的 TourBridge）。
// 标了「可删」的段落可整段拿掉，不影响其余。

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
// 图标编在静态库自己的 qrc 里，没人引用符号就被链接器丢弃（踩过：功能区
// 按钮全空白）。必须留在文件作用域：Q_INIT_RESOURCE 展开是 extern 声明，
// 进匿名命名空间会绑成 internal linkage，链接报找不到
// qInitResources_SARibbonResource
static void InitRibbonResources() { Q_INIT_RESOURCE(SARibbonResource); }
#endif

namespace {

// 组织名/应用名决定数据目录、单实例键与日志文件名，须与安装脚本一致
void SetupAppMetadata(QApplication& app) {
  app.setOrganizationName(QString::fromLatin1(APP_ORGANIZATION_NAME));
  app.setApplicationName(QString::fromLatin1(APP_APPLICATION_NAME));
  app.setWindowIcon(QIcon(":/resources/icons/icon.ico"));
  app.setApplicationVersion(getProjectVersionAll());
}

// 命令行导出崩溃报告（售后兜底）。返回退出码：0 = 成功（含"没有报告"）
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

// 有待处理的崩溃报告就弹导出提示；无桌面环境只记日志（模态框没人点）
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

// 注入 --set 覆盖。须在 Logger::init 之前（log/level 在 init 时读）；那时
// 日志未起，坏条目攒着返回，由调用方事后告警
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
  // 导出报告模式不参与单实例：转发与"因已运行而退出"都让脚本拿到假结果
  if (opts.export_report) {
    return std::nullopt;
  }

  if (!instance.isAppAlreadyRunning()) {
    return std::nullopt;
  }

  if (opts.ui_test_script.isEmpty()) {
    instance.connectToExistingInstance(QCoreApplication::arguments());
    return 0;
  }

  // 回放不豁免单实例，且必须"明确失败"：转发后以 0 退出会让 ctest 误判
  // 通过；起第二实例会抢同一份日志与配置。退出码 2 与回放失败（1）分开。
  // 此刻 Logger 未起只能写 stderr——qWarning 在 Windows 上默认不进管道
  // （ctest 恰恰是管道），消息会丢；QStringLiteral 转 UTF-8 避开 MSVC 对
  // 窄字面量的 C4819
  const QByteArray message =
      QStringLiteral("已有实例在运行，UI 测试回放要求独占：请先关闭程序再跑。")
          .toUtf8();
  std::fprintf(stderr, "%s\n", message.constData());
  return 2;
}

// 装载中文翻译；translator 用静态对象持有（installTranslator 只记指针）。
// 本仓文案全是硬编码中文，要翻的只有第三方控件串，故不问系统语言、一律 zh_CN
void InstallTranslations(QApplication& app) {
  static QTranslator qt_translator;
  static QTranslator saribbon_translator;
  // Qt 自带控件文案；目录 Windows 侧由 windeployqt 拷到 exe 旁
  const QString qt_qm = QLibraryInfo::location(QLibraryInfo::TranslationsPath) +
                        QStringLiteral("/qt_zh_CN.qm");
  if (qt_translator.load(qt_qm)) {
    app.installTranslator(&qt_translator);
    LOG_INFO("MAIN", "已装载 Qt 中文翻译: {}", qt_qm.toStdString());
  } else {
    LOG_WARN("MAIN", "Qt 中文翻译装载失败，标准对话框按钮将显示英文: {}",
             qt_qm.toStdString());
  }

  // 功能区文案：构建期编进资源；LinguistTools 缺席时不存在，降级不拦启动
  const QString saribbon_qm =
      QStringLiteral(":/translations/SARibbon_zh_CN.qm");
  if (saribbon_translator.load(saribbon_qm)) {
    app.installTranslator(&saribbon_translator);
    LOG_INFO("MAIN", "已装载功能区中文翻译: {}", saribbon_qm.toStdString());
  } else {
    LOG_WARN("MAIN", "功能区中文翻译装载失败，功能区自带文案将显示英文");
  }
}

// 单实例激活。「可删」——不需要单实例时整段拿掉（窗口尺寸在 MainWindow 里）
void AttachHostFeatures(SingleInstance& instance, MainWindow& window) {
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
void SetupUiTesting(MainWindow& window) {
  const LaunchOptions& opts = LaunchOptions::instance();
#ifdef APP_ENABLE_QT_TESTING
  // --slow 用 setEventPlaybackDelay 而非 pause 事件：pause 是 sleep，等待期
  // 界面冻结；这个 setter 走 processEventsAndWait，界面照常刷新
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

  // 随主窗口析构；回放模式不挂菜单（没人点），改为立即回放
  auto* ui_test = new UiTestSupport(&window);
  if (opts.ui_test_script.isEmpty()) {
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
  const LaunchOptions& opts = LaunchOptions::instance();

  // ========================= 单实例（可删）=========================
  SingleInstance single_instance(QCoreApplication::applicationName(),
                                 QString::fromLatin1(APP_MUTEX_NAME));
  if (const auto exit_code = HandleAlreadyRunning(single_instance, opts)) {
    return *exit_code;
  }
  // 工具模式不抢首实例的 IPC 服务名
  if (!opts.export_report) {
    single_instance.startListening();
  }

  // ========================= 基础设施 =========================
  // 顺序：配置 → 样式/字体 → 覆盖注入 → Logger → GEH → 崩溃捕获
  {
    // 样式两条路线互斥（APP_UI_ROUTE）：fluent 不加载任何 QSS——样式表一
    // 生效，命中控件就走 QStyleSheetStyle，Fluent 绘制被整个绕过；qss 由
    // ApplyStyleSheet 装底座 + 项目两层。字体两条路都要设
#ifdef APP_UI_FLUENT
    QApplication::setStyle(new FluentUI3Style);
#else
    ApplyStyleSheet(app, {QStringLiteral(":/resources/styles/default.qss")});
#endif
    QFont app_font = app.font();
    app_font.setFamily(QStringLiteral("Microsoft YaHei"));
    app.setFont(app_font);

    ConfigManager::instance();
    const QStringList bad_overrides =
        ApplyConfigOverrides(opts.config_overrides);

#ifdef APP_ENABLE_RIBBON
    // 早于主窗口构造，否则第一帧没有图标
    InitRibbonResources();
#endif

    Logger::init();
    LOG_INFO("MAIN", "应用程序启动，版本: {}", getProjectVersionAll());
    // 本次运行的命令行进日志：事后排查先看这行，就知道进程是怎么被拉起来的
    // （--set 覆盖了什么、是不是回放模式，都在这条记录里）
    LOG_INFO("MAIN", "命令行: {}", CommandLineText().toStdString());
#ifdef APP_UI_FLUENT
    LOG_INFO("MAIN", "界面路线：fluent（FluentUI3Style + Frameless 标题栏）");
#elif defined(APP_ENABLE_RIBBON)
    LOG_INFO("MAIN", "界面路线：qss + SARibbon 功能区（底座 + 项目层样式表）");
#else
    LOG_INFO("MAIN", "界面路线：qss（底座 + 项目层样式表）");
#endif

    // --set 注入时日志未起，这里补汇报
    if (!opts.config_overrides.isEmpty()) {
      LOG_INFO("MAIN", "命令行覆盖配置 {} 项（--set，仅本次运行生效）：{}",
               opts.config_overrides.size(),
               opts.config_overrides.join(QStringLiteral(", ")).toStdString());
      for (const QString& bad : bad_overrides) {
        LOG_WARN("MAIN", "--set 条目非法，已忽略：{}", bad.toStdString());
      }
    }

    // GEH 自己读 crash/backend 决定崩溃信号接不接；后端安装在其后（builtin
    // 的 CrashHandler 要后注册才能接管信号）
    GlobalExceptionHandler::instance().init();

    // 后端选择、handler 定位、结果日志都在 crash::Init 内部；
    // 元信息进 dump 注解与 summary.txt，须由 app 层给（core 拿不到
    // AppVersion.h）
    const crash::AppInfo app_info{QCoreApplication::applicationName(),
                                  QString::fromLatin1(getProjectVersionAll()),
                                  QString::fromLatin1(getProjectGitCommit()),
                                  QString::fromLatin1(APP_BUILD_TYPE)};
    crash::Init(app_info);
  }

  // ========================= 翻译（可删）=========================
  InstallTranslations(app);

  // ========================= 工具模式与崩溃报告提示 =================
  // 导出崩溃报告：无界面，跑完即退出。演练用的"触发测试崩溃"在主窗口
  // 的工具面板/菜单里（MainWindow::onMakeCrash）
  if (opts.export_report) {
    return RunCrashReportExport(opts.export_report_dir);
  }

  // 崩溃那一刻没机会提示，只能等这次启动补上
  if (!opts.export_report && opts.ui_test_script.isEmpty()) {
    MaybePromptCrashReport();
  }

  // ========================= 功能引导（可删）=========================
  // 桥接建在登录前：外部脚本第一步常跑在登录框上，那时主窗口还不存在。
  // 桥接是无 Qt 父级的顶层窗口，用 unique_ptr 持有（控制器挂在 app 下）
  auto* tour_controller = new TourController(&app);
  auto tour_bridge = std::make_unique<TourBridge>(tour_controller->overlay());

  // ========================= 登录 =========================
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

  // ========================= 窗口之后的接线 =========================
  // 引导要 findChild 主窗口里的控件，只能等窗口建好后再接
  tour_controller->setViewport(&w);
  TourTopic* tour_topic = tour_controller->addTopic(BuildTemplateTourTopic(&w));
  auto* tour_menu = new TourMenu(tour_controller, &w);
  // 菜单文字写的是「模板导览」，必须显式传主题，不能靠位置关系
  tour_menu->attachMenu(&w, tour_topic);

  AttachHostFeatures(single_instance, w);
  SetupUiTesting(w);

  // ========================= 主循环与清理 =========================
  const int ret = app.exec();

  GlobalExceptionHandler::instance().shutdown();
  LOG_INFO("MAIN", "应用程序退出");
  Logger::shutdown();
  return ret;
}
