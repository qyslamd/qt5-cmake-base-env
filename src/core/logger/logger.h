#ifndef FILE_CORE_LOGGER_LOGGER_H_
#define FILE_CORE_LOGGER_LOGGER_H_

// 转发包含：日志核心已搬到 src/logging（不依赖 Qt），但 LOG_* 宏、LogLevel
// 等对所有既有调用方仍然从这个头文件可见——全仓 8 处 include 与 66 个 LOG_*
// 调用点因此一行都不用改（见 docs/plan/plan总结.md 的「日志模块拆分」）
#include "logging/log_core.h"

class LogHistoryBuffer;
class QtConsoleSink;

// Qt 侧的日志门面。核心逻辑（sink 组装、模块 logger 注册表、启动期缓冲、
// 关闭）在 LogCore 里；这里只做三件与 Qt 有关的事：
//   1. 用 QStandardPaths / QCoreApplication 算出日志目录与应用名，填 LogInitOptions
//   2. 建 Qt 侧的两个 sink（LogHistoryBuffer + QtConsoleSink）并注入核心
//   3. 订阅 ConfigManager::configChanged，把变更转成 LogCore::ApplyOptions
//
// **方法名与签名刻意保持不变**：main.cpp、log_output_panel.cpp 与
// global_exception_handler.cpp 的调用点因此不用动，这个改动才能干净地
// cherry-pick 到模板的 master 分支。
class Logger {
 public:
  // 初始化日志系统，程序启动时调用一次
  static void init();

  // 关闭日志系统，程序退出时调用一次
  static void shutdown();

  // 获取 Qt 控制台 sink，用于连接 UI 信号
  static QtConsoleSink* qtConsoleSink();

  // 获取启动期历史日志缓冲。Logger 未 init 时返回 nullptr
  static LogHistoryBuffer* qtHistoryBuffer();

 private:
  // 读配置 + 算路径，拼出核心要的启动参数
  static LogInitOptions LoadOptions();

  // 从 ConfigManager 读取格式开关；for_console_sink 决定级别段是否着色
  static LogPatternOptions LoadPatternOptions(bool for_console_sink);
};

#endif  // FILE_CORE_LOGGER_LOGGER_H_
