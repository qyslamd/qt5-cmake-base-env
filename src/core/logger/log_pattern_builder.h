#ifndef FILE_CORE_LOGGER_LOG_PATTERN_BUILDER_H_
#define FILE_CORE_LOGGER_LOG_PATTERN_BUILDER_H_

#include <QString>

// 日志 pattern 的字段开关，每项独立控制一段是否输出。
// 控制台与文件共用同一组开关，只有级别段是否着色不同。
struct LogPatternOptions {
  bool time = true;
  bool level = true;
  bool thread = false;
  bool module = true;
  bool source = false;  // 源文件:行号（%s:%#）
  bool func = false;    // 函数名（%!）
  bool message = true;

  // 控制台 sink 用彩色级别（%^%l%$），文件 sink 用 %l
  bool colored_level = false;
};

// 按开关拼出 spdlog 的 pattern 字符串
QString BuildLogPattern(const LogPatternOptions& options);

// 从 ConfigManager 读取开关（键名见 config_defs.h）。
// for_console_sink 决定级别段是否着色。
LogPatternOptions LoadLogPatternOptionsFromConfig(bool for_console_sink);

#endif  // FILE_CORE_LOGGER_LOG_PATTERN_BUILDER_H_
