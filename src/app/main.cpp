// 应用入口，按序走启动流程。顺序有约束：单实例先于 Logger（拒绝回放时只能
// 写 stderr），引导接线晚于主窗口（要 findChild 窗口里的控件）。
//
// 所有权三种并存：短生命周期用栈对象；活到进程结束的 QObject 挂 Qt 父子；
// 活到进程结束的非 QObject 用 unique_ptr（如 TourBridge）。
// 标了「可删」的段落可整段拿掉，不影响其余。

#include <tour_bridge.h>
#include <tour_controller.h>
#include <tour_menu.h>
#include <tour_style.h>

#include <cstdio>
#include <memory>
#include <optional>

#include <QByteArray>
#include <QDialog>
#include <QString>
#include <QTimer>
#include <QVector>

#include "AppInfo.h"
#include "AppVersion.h"
#include "application.h"
#include "auth/auth_service.h"
#include "common/single_instance.h"
#include "config/config_defs.h"
#include "config/config_manager.h"
#include "crashhandler/crash.h"
#include "crashhandler/global_exception_handler.h"
#include "dialogs/crash_report_dialog.h"
#include "dialogs/login/login_window.h"
#include "dialogs/markdown_window.h"
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

// 引导装配（可删）：建控制器（挂 app）与桥接，并接到窗口上。桥接是顶层控件、
// 要活到进程结束（外部脚本按 "tourBridge" 寻址），所有权交回调用方
std::unique_ptr<TourBridge> SetupTour(MainWindow& window) {
  auto* controller = new TourController(qApp);
  auto bridge = std::make_unique<TourBridge>(controller->overlay());

  // 接线要等窗口建好：viewport 与主题靠 findChild 找窗口里的控件
  controller->setViewport(&window);
  TourTopic* topic = controller->addTopic(BuildTemplateTourTopic(&window));
  auto* menu = new TourMenu(controller, &window);
  // 菜单文字写的是「模板导览」，必须显式传主题，不能靠位置关系
  menu->attachMenu(&window, topic);
  return bridge;
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
  Application app(argc, argv);
  // 引导样式并入应用级样式表——必须在任何窗口之前：它做的是"重设应用表"，
  // 排在窗口级样式表（主窗口的 SARibbon 主题）之后会让主题失效（实测）
  TourStyle::ensureApplied();
  const LaunchOptions& opts = LaunchOptions::instance();

  // ========================= 单实例检测 =========================
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
  // 顺序：配置 → 覆盖注入 → Logger → GEH → 崩溃捕获
  {
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
    // 翻译装载失败项：装载时日志还没起，故攒到这里补记
    for (const QString& problem : app.translationProblems()) {
      LOG_WARN("MAIN", "{}", problem.toStdString());
    }
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

  // ========================= 工具模式与崩溃报告提示 =================
  // 导出崩溃报告：无界面，跑完即退出。退出码契约（export_crash_report.bat
  // 依赖它）：0 = 成功（含"本来就没有报告"），1 = 失败
  if (opts.export_report) {
    const crash::ExportResult result = crash::ExportTo(opts.export_report_dir);
    if (!result.ok()) {
      LOG_WARN("MAIN", "导出崩溃报告失败: {}", result.error.toStdString());
      return 1;
    }
    LOG_INFO("MAIN", "导出崩溃报告 {} 份到 {}", result.exported,
             result.target_dir.toStdString());
    return 0;
  }

  // 崩溃那一刻没机会提示，只能等这次启动补上
  if (!opts.export_report && opts.ui_test_script.isEmpty()) {
    PromptPendingCrashReportIfAny();
  }

  // ========================= 登录 =========================
  if (opts.ui_test_script.isEmpty() &&
      ConfigManager::instance().get(kConfigLoginEnabled, true)) {
    LoginWindow login;
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
  // 桥接要活到进程结束，这里接住就是它的寿命
  auto tour_bridge = SetupTour(w);
  AttachHostFeatures(single_instance, w);
  SetupUiTesting(w);

  // --doc：启动即把这份文档开出来（脚本与 funq 探针的入口，不必点菜单）。
  // 窗口挂主窗口做父级，随主窗口一起销毁；unique_ptr 先于 w 析构，不会双删
  std::unique_ptr<MarkdownWindow> doc_window;
  if (!opts.doc_path.isEmpty()) {
    doc_window = std::make_unique<MarkdownWindow>(&w);
    doc_window->OpenFile(opts.doc_path);
  }

  // ========================= 主循环与清理 =========================
  const int ret = app.exec();

  GlobalExceptionHandler::instance().shutdown();
  LOG_INFO("MAIN", "应用程序退出");
  Logger::shutdown();
  return ret;
}
