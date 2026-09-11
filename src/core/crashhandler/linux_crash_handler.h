#include <QtGlobal>
#ifndef Q_OS_WIN

#ifndef FILE_CORE_CRASHHANDLER_LINUX_CRASH_HANDLER_H_
#define FILE_CORE_CRASHHANDLER_LINUX_CRASH_HANDLER_H_

#include <csignal>
#include <cstddef>
#include <functional>

#include "crash_handler.h"

/// Linux 平台崩溃处理器
///
/// 基于 sigaction + sigaltstack + backtrace 实现 async-signal-safe 的
/// 崩溃日志生成。信号处理器中零堆分配，仅使用栈缓冲和预计算数据。
class LinuxCrashHandler : public CrashHandler {
 public:
  LinuxCrashHandler();
  ~LinuxCrashHandler() override;

  bool init() override;
  void setDumpPath(const QString& path) override;

  /// @note Linux 平台下回调不会在信号处理器中执行（async-signal-safe
  ///       约束，std::function 不能安全调用）。崩溃信息仅写入日志文件。
  ///       回调保留用于未来 fork 子进程方案。
  void setCrashCallback(
      std::function<void(const QString& crashLog)> callback) override;

  /// 格式化信号头部信息到给定缓冲区（可测试的纯函数）
  /// @return 写入的字节数，失败返回 0
  static size_t formatSignalHeader(char* buf, size_t bufSize, int signum,
                                   siginfo_t* info);

 private:
  // 信号处理器（async-signal-safe）
  static void signalHandler(int signum, siginfo_t* info, void* context);

  // Tier 1：信号安全核心逻辑（零堆分配）
  static void doCrashDump(int signum, siginfo_t* info);

  // 获取信号名称（async-signal-safe）
  static const char* getSignalName(int signum);

  // 预存状态（init 时填充，信号处理器中只读）
  static LinuxCrashHandler* s_instance;
  static volatile sig_atomic_t s_inHandler;

  // 备用栈（init 时分配，用于栈溢出场景）
  void* altstack_mem_ = nullptr;
  stack_t old_altstack_ = {};

  char dump_path_[4096] = {0};
  char precomputed_info_[4096] = {0};
  size_t precomputed_info_len_ = 0;
  std::function<void(const QString&)> crash_callback_;

  // 保存旧信号处理器，析构时恢复
  struct sigaction old_actions_[_NSIG];
  bool initialized_ = false;

  // 目标信号列表
  static constexpr int kCrashSignals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGILL,
                                          SIGBUS};
  static constexpr int kNumCrashSignals = 5;
};

#endif  // FILE_CORE_CRASHHANDLER_LINUX_CRASH_HANDLER_H_

#endif  // !Q_OS_WIN
