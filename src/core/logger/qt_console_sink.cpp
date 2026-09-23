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
  // 格式化器会在末尾补 spdlog 的行尾（Windows 是 "\r\n"），那是给按行读的 sink
  // 用的。Qt 侧每条消息自成一个文本块，留着它就会多渲染出空行——面板的条目样式
  // 带 white-space:pre-wrap，换行不会再被折叠掉
  while (!text.isEmpty() && (text.endsWith(QLatin1Char('\n')) ||
                             text.endsWith(QLatin1Char('\r')))) {
    text.chop(1);
  }
  if (history_ != nullptr) {
    history_->push(static_cast<int>(msg.level), text);
  }
  emit logMessage(static_cast<int>(msg.level), text);
}

void QtConsoleSink::flush_() {}
