#include "log_history_buffer.h"

#include <QPointer>

LogHistoryBuffer::LogHistoryBuffer(int capacity, QObject* parent)
    : QObject(parent), capacity_(capacity) {}

LogHistoryBuffer::~LogHistoryBuffer() = default;

void LogHistoryBuffer::push(int level, const QString& text) {
  QMutexLocker locker(&mutex_);
  if (buffer_.size() >= static_cast<size_t>(capacity_)) {
    buffer_.pop_front();
  }
  buffer_.push_back(LogEntry{level, text});
}

void LogHistoryBuffer::drain(QObject* receiver) {
  QList<LogEntry> snapshot;
  {
    QMutexLocker locker(&mutex_);
    if (drained_) {
      return;
    }
    drained_ = true;
    snapshot.reserve(static_cast<int>(buffer_.size()));
    for (const auto& entry : buffer_) {
      snapshot.append(entry);
    }
  }
  // 锁外 emit。QPointer 守 receiver 避免 dangling。
  QPointer<QObject> guard(receiver);
  if (guard) {
    emit drained(snapshot);
  }
}
