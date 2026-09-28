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
// 与各崩溃后端的职责边界（这就是 init() 那个开关的由来，见 APP_CRASH_BACKEND）：
//   - builtin（自研）：崩溃信号实际由 CrashHandler 接管——它在本类之后注册，
//     Windows 侧更是 VEH 先响应，本类只是二线兜底，照常注册
//   - crashpad：崩溃信号必须让给 crashpad_handler 的进程外捕获，本类只留
//     SIGINT/SIGTERM，故 main 传 enable_crash_signals=false
//   - none：本仓不装捕获，也传 false，把崩溃信号留给使用者自己的方案
class GlobalExceptionHandler : public QObject {
  Q_OBJECT

 public:
  static GlobalExceptionHandler& instance();

  // 初始化。enable_crash_signals=false 时只注册 SIGINT/SIGTERM（进程终止类）。
  //
  // Crashpad 模式下必须传 false：Crashpad 的 crashpad_handler 通过进程外
  // 机制捕获崩溃，如果这里抢先用 std::signal 接住崩溃信号，handler 进程
  // 收不到通知，dump 也就不会生成。
  void init(bool enable_crash_signals = true);
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
