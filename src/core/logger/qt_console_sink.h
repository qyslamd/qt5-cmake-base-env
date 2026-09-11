#ifndef FILE_CORE_LOGGER_QT_CONSOLE_SINK_H_
#define FILE_CORE_LOGGER_QT_CONSOLE_SINK_H_

#include <mutex>
#include <spdlog/sinks/base_sink.h>

#include <QObject>

class LogHistoryBuffer;

// spdlog 自定义 sink：把日志同时写入 LogHistoryBuffer 并通过 Qt 信号发出，
// 供 UI 日志面板消费。运行于 spdlog 工作线程。
class QtConsoleSink : public QObject,
                      public spdlog::sinks::base_sink<std::mutex> {
  Q_OBJECT

 public:
  explicit QtConsoleSink(LogHistoryBuffer* history = nullptr,
                         QObject* parent = nullptr);
  ~QtConsoleSink() override;

 signals:
  void logMessage(int level, const QString& formatted_text);

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override;
  void flush_() override;

 private:
  LogHistoryBuffer* history_ = nullptr;
};

#endif  // FILE_CORE_LOGGER_QT_CONSOLE_SINK_H_
