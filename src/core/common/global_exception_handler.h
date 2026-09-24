#ifndef FILE_CORE_COMMON_GLOBAL_EXCEPTION_HANDLER_H_
#define FILE_CORE_COMMON_GLOBAL_EXCEPTION_HANDLER_H_

#include <QObject>
#include <QString>

class GlobalExceptionHandler : public QObject {
  Q_OBJECT

 public:
  static GlobalExceptionHandler& instance();

  // 初始化。enable_crash_signals=false 时只注册 SIGINT/SIGTERM（进程终止类），
  // 不注册 SIGSEGV/SIGABRT/SIGFPE/SIGILL/SIGBUS（崩溃类）。
  //
  // Crashpad 模式下必须传 false：Crashpad 的 crashpad_handler 通过进程外
  // 机制捕获崩溃，如果这里抢先用 std::signal 接住崩溃信号，handler 进程
  // 收不到通知，dump 也就不会生成。
  void init(bool enable_crash_signals = true);
  void shutdown();

 signals:
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
