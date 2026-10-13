#ifndef FILE_CORE_CRASHHANDLER_GLOBAL_EXCEPTION_HANDLER_H_
#define FILE_CORE_CRASHHANDLER_GLOBAL_EXCEPTION_HANDLER_H_

#include <QObject>
#include <QString>

// 进程级异常兜底。这个类管两件事，别只看名字：
//   1. 信号 —— 始终注册 SIGINT/SIGTERM；崩溃类信号（SIGSEGV / SIGABRT /
//      SIGFPE / SIGILL / SIGBUS）是否注册由 init() 的开关决定
//   2. Qt 消息重定向 —— qInstallMessageHandler 把 qDebug/qWarning/qCritical
//      接进本仓的 spdlog 日志，否则 Qt 自己的消息没有落点
// 两件事共用同一次 init()/shutdown()（装与卸成对），所以放在一个类里，
// 而不是拆成两个各管一半
//
// 与各崩溃后端的职责边界（后端是运行时配置 crash/backend，本类在 init() 里
// 自己读它决定崩溃信号接不接，见 crash.h）：
//   - builtin（自研）：崩溃信号实际由 CrashHandler 接管——它在本类之后注册，
//     Windows 侧更是 VEH 先响应，本类只是二线兜底，照常注册
//   - crashpad：崩溃信号必须让给 crashpad_handler 的进程外捕获，本类只留
//     SIGINT/SIGTERM
//   - none：本仓不装捕获，崩溃信号留给使用者自己的方案
class GlobalExceptionHandler : public QObject {
  Q_OBJECT

 public:
  static GlobalExceptionHandler& instance();

  // 初始化：始终注册 SIGINT/SIGTERM（进程终止类）；崩溃类信号是否注册由
  // 配置 crash/backend 决定——非 builtin 一律不注册，否则 crashpad 的
  // crashpad_handler 进程收不到通知，dump 不会生成
  void init();
  void shutdown();

 signals:
  // 崩溃消息与调用栈。当前没有消费者：留着当扩展点（接一个崩溃提示界面或
  // 上报都行），emit 时没有接收者是空操作
  void exceptionCaught(const QString& message, const QString& stackTrace);

 private:
  GlobalExceptionHandler();
  ~GlobalExceptionHandler() override;

  static void signalHandler(int signal);
  static void qtMessageHandler(QtMsgType type,
                               const QMessageLogContext& context,
                               const QString& message);

  void setupSignalHandlers();
  void restoreSignalHandlers();

  static QList<int> s_signals_;
  static QList<void (*)(int)> s_oldHandlers_;
  bool initialized_ = false;
};

#endif  // FILE_CORE_COMMON_GLOBAL_EXCEPTION_HANDLER_H_
