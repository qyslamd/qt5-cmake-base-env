#ifndef FILE_CORE_LOGGER_LOG_HISTORY_BUFFER_H_
#define FILE_CORE_LOGGER_LOG_HISTORY_BUFFER_H_

#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>

#include <deque>

// 单条历史日志。POD 结构，构造时不分配资源。
struct LogEntry {
  int level;     // spdlog::level::level_enum 强转 int
  QString text;  // 格式化后的文本（QtConsoleSink formatter 之后）
};

// 线程安全、定容的环形日志缓冲。
// - push()：由 spdlog 工作线程调用，容量满则丢最老。
// - drain()：由 UI 线程调用，一次性把当前快照通过 drained 信号发给
//   receiver。调用后内部 drained_ 置 true，再次调用为 noop。
class LogHistoryBuffer : public QObject {
  Q_OBJECT

 public:
  explicit LogHistoryBuffer(int capacity = 5000, QObject* parent = nullptr);
  ~LogHistoryBuffer() override;

  // spdlog 工作线程调用：写入一条历史。容量满则丢最老。
  void push(int level, const QString& text);

  // UI 线程调用：把当前所有历史快照通过 drained 信号一次性发给 receiver。
  // 调一次后置 drained_=true，再次调用为 noop。receiver 失效则不发信号。
  void drain(QObject* receiver);

 signals:
  // drain 触发后同步 emit（DirectConnection）。
  // 携带的 entries 是 drain 调用瞬间的 buffer 快照。
  void drained(const QList<LogEntry>& entries);

 private:
  QMutex mutex_;
  std::deque<LogEntry> buffer_;
  int capacity_;
  bool drained_ = false;
};

#endif  // FILE_CORE_LOGGER_LOG_HISTORY_BUFFER_H_
