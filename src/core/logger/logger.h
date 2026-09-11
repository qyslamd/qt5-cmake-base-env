#ifndef FILE_CORE_LOGGER_LOGGER_H_
#define FILE_CORE_LOGGER_LOGGER_H_

#include <spdlog/spdlog.h>
#include <unordered_map>

#include <QMutex>
#include <QString>

class LogHistoryBuffer;
class QtConsoleSink;

// 日志级别枚举，与 spdlog 级别一一对应
enum LogLevel {
  LOG_LEVEL_DEBUG = spdlog::level::debug,
  LOG_LEVEL_INFO = spdlog::level::info,
  LOG_LEVEL_WARN = spdlog::level::warn,
  LOG_LEVEL_ERROR = spdlog::level::err,
  LOG_LEVEL_FATAL = spdlog::level::critical
};

// 日志系统对外接口：初始化 spdlog、管理模块 logger、维护 UI 输出 sink
class Logger {
 public:
  // 初始化日志系统，程序启动时调用一次
  static void init();

  // 关闭日志系统，程序退出时调用一次
  static void shutdown();

  // 设置指定模块的日志级别
  static void setLevel(const QString& module, LogLevel level);

  // 设置所有模块的日志级别
  static void setAllLevel(LogLevel level);

  // 获取 Qt 控制台 sink，用于连接 UI 信号
  static QtConsoleSink* qtConsoleSink();

  // 获取启动期历史日志缓冲。Logger 未 init 时返回 nullptr。
  static LogHistoryBuffer* qtHistoryBuffer();

  // 获取模块 logger 指针，供 LOG 宏使用
  static spdlog::logger* getLogger(const QString& module);

 private:
  // 按需创建模块 logger，线程安全（调用方需持有 s_mutex_）
  static spdlog::logger* getOrCreateModuleLogger(
      const std::string& module_name);

  // 按当前配置重建全部 sink 的 pattern（格式开关变更时调用）
  static void ApplyPatternToSinks();

  static bool s_initialized_;
  static QMutex s_mutex_;
  static QtConsoleSink* s_qt_sink_;
  static LogHistoryBuffer* s_history_buffer_;
  static std::unordered_map<std::string, spdlog::logger*> s_module_loggers_;
  // 保留控制台/文件 sink 句柄，供格式开关变更后重建 pattern
  static spdlog::sink_ptr s_console_sink_;
  static spdlog::sink_ptr s_file_sink_;
};

// 全局日志调用宏，使用 fmt 格式化语法 ({})
#define LOG_DEBUG(module, ...)                            \
  do {                                                    \
    auto* _l = Logger::getLogger(module);                 \
    if (_l) {                                             \
      _l->log(spdlog::source_loc{__FILE__, __LINE__, ""}, \
              spdlog::level::debug, __VA_ARGS__);         \
    }                                                     \
  } while (0)

#define LOG_INFO(module, ...)                                                  \
  do {                                                                         \
    auto* _l = Logger::getLogger(module);                                      \
    if (_l) {                                                                  \
      _l->log(spdlog::source_loc{__FILE__, __LINE__, ""}, spdlog::level::info, \
              __VA_ARGS__);                                                    \
    }                                                                          \
  } while (0)

#define LOG_WARN(module, ...)                                                  \
  do {                                                                         \
    auto* _l = Logger::getLogger(module);                                      \
    if (_l) {                                                                  \
      _l->log(spdlog::source_loc{__FILE__, __LINE__, ""}, spdlog::level::warn, \
              __VA_ARGS__);                                                    \
    }                                                                          \
  } while (0)

#define LOG_ERROR(module, ...)                                                \
  do {                                                                        \
    auto* _l = Logger::getLogger(module);                                     \
    if (_l) {                                                                 \
      _l->log(spdlog::source_loc{__FILE__, __LINE__, ""}, spdlog::level::err, \
              __VA_ARGS__);                                                   \
    }                                                                         \
  } while (0)

#define LOG_FATAL(module, ...)                            \
  do {                                                    \
    auto* _l = Logger::getLogger(module);                 \
    if (_l) {                                             \
      _l->log(spdlog::source_loc{__FILE__, __LINE__, ""}, \
              spdlog::level::critical, __VA_ARGS__);      \
    }                                                     \
  } while (0)

#endif  // FILE_CORE_LOGGER_LOGGER_H_
