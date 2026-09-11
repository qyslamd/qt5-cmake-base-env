#include "qt_console_sink.h"

#include "log_history_buffer.h"

QtConsoleSink::QtConsoleSink(LogHistoryBuffer* history, QObject* parent)
    : QObject(parent), history_(history) {
  set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
}

QtConsoleSink::~QtConsoleSink() = default;

void QtConsoleSink::sink_it_(const spdlog::details::log_msg& msg) {
  spdlog::memory_buf_t formatted;
  formatter_->format(msg, formatted);
  QString text =
      QString::fromUtf8(formatted.data(), static_cast<int>(formatted.size()));
  if (history_ != nullptr) {
    history_->push(static_cast<int>(msg.level), text);
  }
  emit logMessage(static_cast<int>(msg.level), text);
}

void QtConsoleSink::flush_() {}
