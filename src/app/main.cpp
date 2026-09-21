#include <cstdio>

#include <QApplication>
#include <QByteArray>
#include <QDialog>
#include <QFile>
#include <QIcon>
#include <QTimer>

#include <qt-libui/splash_screen.h>

#include "AppInfo.h"
#include "AppVersion.h"
#include "auth/auth_service.h"
#include "common/global_exception_handler.h"
#include "common/single_instance.h"
#include "config/config_defs.h"
#include "config/config_manager.h"
#include "crashhandler/crash_handler.h"
#include "dialogs/login_dialog.h"
#include "logger/logger.h"
#include "mainwindow.h"
#ifdef APP_ENABLE_QT_TESTING
#include "qttesting/testing_support.h"
#endif

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  // 组织名/应用名来自 CMake，决定 %LOCALAPPDATA% 数据目录、单实例键与
  // 日志文件名，须与安装脚本的卸载路径保持一致
  app.setOrganizationName(QString::fromLatin1(APP_ORGANIZATION_NAME));
  app.setApplicationName(QString::fromLatin1(APP_APPLICATION_NAME));
  app.setWindowIcon(QIcon(":/resources/icons/icon.ico"));
  app.setApplicationVersion(getProjectVersionAll());

  // UI 测试回放入口：--play <脚本>（详见 src/app/qttesting/ui_test_support.h）
  QString ui_test_script;
  {
    const QStringList app_args = QCoreApplication::arguments();
    const int play_index = app_args.indexOf(QStringLiteral("--play"));
    if (play_index >= 0 && play_index + 1 < app_args.size()) {
      ui_test_script = app_args.at(play_index + 1);
    }
  }

  // 单实例检测：appKey 随 applicationName 走，互斥体名由 CMake 提供
  // （与安装脚本同源），第二实例转发参数后退出
  SingleInstance single_instance(QCoreApplication::applicationName(),
                                 QString::fromLatin1(APP_MUTEX_NAME));
  if (single_instance.isAppAlreadyRunning()) {
    if (ui_test_script.isEmpty()) {
      // 普通启动的第二实例：把命令行转交给首实例，自己退出
      single_instance.connectToExistingInstance(QCoreApplication::arguments());
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
    // 后者会让 MSVC 报 C4819（窄字面量要落到执行字符集上），而前者是 UTF-16，
    // 不涉及这个问题
    const QByteArray message =
        QStringLiteral("已有实例在运行，UI 测试回放要求独占：请先关闭程序再跑。")
            .toUtf8();
    std::fprintf(stderr, "%s\n", message.constData());
    return 2;
  }
  single_instance.startListening();

  // 加载全局样式
  QFile style_file(QStringLiteral(":/resources/styles/default.qss"));
  if (style_file.open(QIODevice::ReadOnly)) {
    app.setStyleSheet(QString::fromUtf8(style_file.readAll()));
  }

  // 配置管理必须先于 Logger 初始化（Logger 读取日志配置）
  ConfigManager::instance();

  Logger::init();
  LOG_INFO("MAIN", "应用程序启动，版本: {}", getProjectVersionAll());

  // 信号捕获 + Qt 消息重定向。致命信号实际由 CrashHandler 接管
  // （其注册晚于本模块），此处生效的是 Qt 消息重定向与 SIGINT/SIGTERM
  GlobalExceptionHandler::instance().init();

  // 初始化崩溃捕获模块
  auto crash_handler = CrashHandler::create();
  if (crash_handler) {
    crash_handler->init();
    LOG_INFO("MAIN", "崩溃捕获模块初始化完成");
  }

  // 登录成功前不创建主窗口。回放模式跳过登录：录制入口在主窗口菜单里，
  // 也就是录制总是从「已登录」状态开始；回放只有从同一状态起步，脚本里
  // 记录的控件才存在
  if (ui_test_script.isEmpty()) {
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
             ui_test_script.toStdString());
  }

  // 启动阶段：Splash 覆盖主界面创建过程。各步骤之间插入
  // processEvents 保证进度条实时刷新；模板使用者可在步骤之间加入
  // 自己的耗时初始化逻辑，Splash 无需感知主窗口内部结构。
  SplashScreen splash;
  // logo 由消费方提供（库不携带资源），用应用图标
  splash.setLogo(QIcon(":/resources/icons/icon.ico"));
  splash.show();
  QCoreApplication::processEvents();

  splash.setStatusText(QStringLiteral("初始化用户会话"));
  splash.setProgress(25);
  QCoreApplication::processEvents();
  const User current_user = AuthService::instance().currentUser();
  LOG_INFO("MAIN", "用户会话已建立: {}", current_user.user_name_.toStdString());

  splash.setStatusText(QStringLiteral("加载用户配置"));
  splash.setProgress(50);
  QCoreApplication::processEvents();
  // 钳制窗口尺寸，防止损坏的 INI 产生不可用窗口
  const int saved_width = ConfigManager::instance().get<int>(
      QString::fromLatin1(kConfigWindowWidth), kConfigWindowDefaultWidth);
  const int saved_height = ConfigManager::instance().get<int>(
      QString::fromLatin1(kConfigWindowHeight), kConfigWindowDefaultHeight);
  const int window_width = qBound(640, saved_width, 4096);
  const int window_height = qBound(480, saved_height, 2160);

  splash.setStatusText(QStringLiteral("创建主界面"));
  splash.setProgress(80);
  QCoreApplication::processEvents();
  MainWindow w;
  w.resize(window_width, window_height);

  splash.setStatusText(QStringLiteral("启动完成"));
  splash.setProgress(100);
  QCoreApplication::processEvents();
  splash.finish();

  w.show();

#ifdef APP_ENABLE_QT_TESTING
  // 该对象随主窗口一起析构；回放模式下不挂菜单（没人点），改为立即回放
  auto* ui_test = new UiTestSupport(&w);
  if (ui_test_script.isEmpty()) {
    // BUILD_QT_TESTING 打开的构建带「测试 → 录制/回放」菜单
    ui_test->attachMenu(&w);
  } else {
    // 窗口已显示再回放，事件才有落点；播完即退出，退出码就是测试结果
    QTimer::singleShot(0, &w, [ui_test, ui_test_script]() {
      const bool played = ui_test->playFile(ui_test_script);
      QCoreApplication::exit(played ? 0 : 1);
    });
  }
#else
  if (!ui_test_script.isEmpty()) {
    LOG_WARN("MAIN", "--play 需要 BUILD_QT_TESTING=ON 的构建，已忽略: {}",
             ui_test_script.toStdString());
  }
#endif

  // 第二实例请求激活：Windows 在 SingleInstance 内走 HWND 置前，
  // 其余平台依赖本信号完成 Qt 层的显示与激活
  single_instance.setActivationWindow(reinterpret_cast<void*>(w.winId()));
  QObject::connect(&single_instance, &SingleInstance::showApplication, &w,
                   [&w]() {
                     w.showNormal();
                     w.raise();
                     w.activateWindow();
                   });

  // 退出前保存窗口尺寸并立即落盘
  QObject::connect(&app, &QApplication::aboutToQuit, [&w]() {
    auto& cfg = ConfigManager::instance();
    cfg.set<int>(QString::fromLatin1(kConfigWindowWidth), w.width());
    cfg.set<int>(QString::fromLatin1(kConfigWindowHeight), w.height());
    cfg.sync();
  });

  int ret = app.exec();

  GlobalExceptionHandler::instance().shutdown();
  LOG_INFO("MAIN", "应用程序退出");
  Logger::shutdown();
  return ret;
}
