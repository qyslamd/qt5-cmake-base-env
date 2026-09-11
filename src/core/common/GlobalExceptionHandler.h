#ifndef ETEST_CORE_COMMON_GLOBAL_EXCEPTION_HANDLER_H_
#define ETEST_CORE_COMMON_GLOBAL_EXCEPTION_HANDLER_H_

#include <QObject>
#include <QString>

class GlobalExceptionHandler : public QObject {
  Q_OBJECT

 public:
  static GlobalExceptionHandler& instance();

  void init();
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

#endif  // ETEST_CORE_COMMON_GLOBAL_EXCEPTION_HANDLER_H_
