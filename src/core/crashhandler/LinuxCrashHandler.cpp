#include <QtGlobal>
#ifndef Q_OS_WIN

#include "LinuxCrashHandler.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

#include <cerrno>
#include <cstring>
#include <limits>

#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
// dlfcn.h 预留：未来 V2 版本用 dladdr() 解析符号名

#include "logger/Logger.h"

namespace {
// 崩溃日志文件名片段（编译期常量，sizeof 含 '\0'）
constexpr char kCrashFilePrefix[] = "/etest_crash_";
constexpr char kCrashFileExt[] = ".log";

// long long 十进制最大位数（含符号位）
constexpr size_t kTimestampMaxLen =
    std::numeric_limits<long long>::digits10 + 1;

// 终端提示文本片段
constexpr char kTermCrashPrefix[] = "\n[ETestStudio] 程序崩溃 (";
constexpr char kTermCrashMid[] = ")\n崩溃日志已写入: ";
constexpr char kTermCrashSuffix[] = "\n使用 addr2line 解析调用栈地址。\n";

/// 用 write 输出 C 字符串字面量，自动推导长度
inline void writeStr(int fd, const char* s) {
  write(fd, s, strlen(s));
}
}  // namespace

// 静态成员定义
LinuxCrashHandler* LinuxCrashHandler::s_instance = nullptr;
volatile sig_atomic_t LinuxCrashHandler::s_inHandler = 0;

// 目标信号列表
constexpr int LinuxCrashHandler::kCrashSignals[];
constexpr int LinuxCrashHandler::kNumCrashSignals;

LinuxCrashHandler::LinuxCrashHandler() {
  s_instance = this;
  // 默认崩溃日志路径：AppLocalDataLocation/crash/
  QString localPath =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  QString crashPath = localPath + "/crash/";
  QDir().mkpath(crashPath);
  std::strncpy(dump_path_, crashPath.toUtf8().constData(),
               sizeof(dump_path_) - 1);
  dump_path_[sizeof(dump_path_) - 1] = '\0';
}

LinuxCrashHandler::~LinuxCrashHandler() {
  if (initialized_) {
    // 恢复旧信号处理器
    for (int i = 0; i < kNumCrashSignals; ++i) {
      int sig = kCrashSignals[i];
      if (sig < _NSIG) {
        sigaction(sig, &old_actions_[sig], nullptr);
      }
    }
    // 恢复原备用栈
    sigaltstack(&old_altstack_, nullptr);
  }
  // 释放备用栈内存
  if (altstack_mem_) {
    free(altstack_mem_);
    altstack_mem_ = nullptr;
  }
  s_instance = nullptr;
}

bool LinuxCrashHandler::init() {
  if (initialized_) {
    return true;
  }

  // 预计算系统+程序信息到固定缓冲
  QString commonInfo = collectCommonInfo();
  QByteArray infoUtf8 = commonInfo.toUtf8();
  precomputed_info_len_ = std::min(static_cast<size_t>(infoUtf8.size()),
                                   sizeof(precomputed_info_) - 1);
  std::memcpy(precomputed_info_, infoUtf8.constData(), precomputed_info_len_);
  precomputed_info_[precomputed_info_len_] = '\0';

  // 分配备用栈（用于栈溢出场景）
  altstack_mem_ = malloc(SIGSTKSZ);
  if (!altstack_mem_) {
    LOG_ERROR("CRASH", "sigaltstack 内存分配失败");
    return false;
  }

  stack_t altstack;
  altstack.ss_sp = altstack_mem_;
  altstack.ss_size = SIGSTKSZ;
  altstack.ss_flags = 0;
  if (sigaltstack(&altstack, &old_altstack_) != 0) {
    LOG_ERROR("CRASH", "sigaltstack 设置失败: {}", strerror(errno));
    free(altstack_mem_);
    altstack_mem_ = nullptr;
    return false;
  }

  // 注册信号处理器
  // SA_RESETHAND: 信号处理器执行后恢复默认行为，防止 doCrashDump 内部
  // 嵌套崩溃导致无限递归（嵌套信号走默认处理器直接终止进程）
  struct sigaction sa;
  sa.sa_sigaction = signalHandler;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
  sigemptyset(&sa.sa_mask);

  for (int i = 0; i < kNumCrashSignals; ++i) {
    int sig = kCrashSignals[i];
    if (sigaction(sig, &sa, &old_actions_[sig]) != 0) {
      LOG_ERROR("CRASH", "sigaction({}) 注册失败: {}", sig, strerror(errno));
    }
  }

  initialized_ = true;
  LOG_INFO("CRASH", "Linux 崩溃处理器初始化完成，日志目录: {}", dump_path_);
  return true;
}

void LinuxCrashHandler::setDumpPath(const QString& path) {
  QDir().mkpath(path);
  std::strncpy(dump_path_, path.toUtf8().constData(), sizeof(dump_path_) - 1);
  dump_path_[sizeof(dump_path_) - 1] = '\0';
}

void LinuxCrashHandler::setCrashCallback(
    std::function<void(const QString& crashLog)> callback) {
  crash_callback_ = std::move(callback);
}

const char* LinuxCrashHandler::getSignalName(int signum) {
  switch (signum) {
    case SIGSEGV:
      return "SIGSEGV";
    case SIGABRT:
      return "SIGABRT";
    case SIGFPE:
      return "SIGFPE";
    case SIGILL:
      return "SIGILL";
    case SIGBUS:
      return "SIGBUS";
    default:
      return "UNKNOWN";
  }
}

size_t LinuxCrashHandler::formatSignalHeader(char* buf,
                                             size_t bufSize,
                                             int signum,
                                             siginfo_t* info) {
  if (!buf || bufSize == 0) {
    return 0;
  }

  const char* name = getSignalName(signum);

  // si_addr 仅对硬件异常信号有意义，SIGABRT 无意义
  if (signum == SIGABRT) {
    return static_cast<size_t>(snprintf(
        buf, bufSize, "=== 崩溃信息 ===\n信号: %s (%d)\nsi_code: %d\n\n", name,
        signum, info ? info->si_code : 0));
  }

  void* faultAddr = info ? info->si_addr : nullptr;
  return static_cast<size_t>(
      snprintf(buf, bufSize,
               "=== 崩溃信息 ===\n信号: %s (%d)\nsi_code: %d\n故障地址: %p\n\n",
               name, signum, info ? info->si_code : 0, faultAddr));
}

void LinuxCrashHandler::doCrashDump(int signum, siginfo_t* info) {
  // 1. 获取 backtrace
  void* buffer[64];
  int frames = backtrace(buffer, 64);

  // 2. 组装文件路径：dump_path_ + kCrashFilePrefix + 时间戳 + kCrashFileExt
  //    所有尺寸编译期推导，无魔数
  char filepath[sizeof(s_instance->dump_path_) + sizeof(kCrashFilePrefix) - 1 +
                kTimestampMaxLen + sizeof(kCrashFileExt) - 1];
  time_t now = time(nullptr);
  snprintf(filepath, sizeof(filepath), "%s%s%lld%s", s_instance->dump_path_,
           kCrashFilePrefix, static_cast<long long>(now), kCrashFileExt);

  // 3. 打开日志文件
  int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    // 降级：写入 stderr
    writeStr(STDERR_FILENO, "CrashHandler: 无法创建崩溃日志文件\n");
    return;
  }

  // 4. 写入信号头部
  char header[512];
  size_t headerLen = formatSignalHeader(header, sizeof(header), signum, info);
  if (headerLen > 0) {
    write(fd, header, headerLen);
  }

  // 5. 写入帧数信息
  char frameInfo[128];
  int frameLen = snprintf(frameInfo, sizeof(frameInfo),
                          "=== 调用栈 (%d 帧) ===\n", frames);
  if (frameLen > 0) {
    write(fd, frameInfo, static_cast<size_t>(frameLen));
  }

  // 6. 写入预计算系统信息
  if (s_instance->precomputed_info_len_ > 0) {
    write(fd, s_instance->precomputed_info_, s_instance->precomputed_info_len_);
  }

  // 7. backtrace_symbols_fd 直接写 fd（零分配）
  backtrace_symbols_fd(buffer, frames, fd);

  // 8. 写入回调提示
  writeStr(fd,
           "\n=== 提示 ===\n回调因信号安全约束未执行，请查看日志文件。\n"
           "使用 addr2line -e <可执行文件> -f -C <地址> 解析调用栈。\n");

  close(fd);

  // 9. 输出到终端：用多次 write 替代 snprintf 拼接，零缓冲零魔数
  writeStr(STDERR_FILENO, kTermCrashPrefix);
  writeStr(STDERR_FILENO, getSignalName(signum));
  writeStr(STDERR_FILENO, kTermCrashMid);
  writeStr(STDERR_FILENO, filepath);
  writeStr(STDERR_FILENO, kTermCrashSuffix);
}

void LinuxCrashHandler::signalHandler(int signum,
                                      siginfo_t* info,
                                      void* /*context*/) {
  // s_inHandler 防 doCrashDump 返回后、_exit 之前被同一信号再次中断。
  // 注意：由于 SA_RESETHAND，doCrashDump 内部的嵌套崩溃不会回到此函数，
  // 而是走默认处理器直接终止。s_inHandler 仅覆盖 doCrashDump 之后路径。
  if (s_inHandler) {
    _exit(128 + signum);
  }
  s_inHandler = 1;

  if (s_instance) {
    doCrashDump(signum, info);
  }

  // 不走 atexit，避免二次崩溃
  _exit(128 + signum);
}
#endif  // !Q_OS_WIN
