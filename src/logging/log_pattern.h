#ifndef FILE_LOG_LOG_PATTERN_H_
#define FILE_LOG_LOG_PATTERN_H_

#include <string>

#include "logging/log_types.h"

// 按开关拼出 spdlog 的 pattern 字符串。
//
// 只做"怎么拼"，不管"开关从哪来"——从配置读取那半边在 Qt 桥里
// （ConfigManager 是 Qt 的），核心这边保持干净
std::string BuildLogPattern(const LogPatternOptions& options);

#endif  // FILE_LOG_LOG_PATTERN_H_
