#include "crashhandler/global_exception_handler.h"

#include <csignal>
#include <cstdlib>

#include <QDebug>

#include "config/config_defs.h"
#include "config/config_manager.h"
#include "logger/logger.h"

#ifdef Q_OS_WIN
// clang-format off
// windows.h 必须先于 dbghelp.h，禁止 clang-format 重排
#include <windows.h>
#include <dbghelp.h>
// clang-format on
#else
#include <execinfo.h>
#endif

namespace {

// 崩溃信号：由崩溃捕获模块（自研 CrashHandler 或 Crashpad）负责，
// GlobalExceptionHandler 只在未开启崩溃捕获时兜底
const QList<int> kCrashSignals = {
    SIGABRT, SIGFPE, SIGILL, SIGSEGV,
#ifndef Q_OS_WIN
    SIGBUS,
#endif
};

// 终止信号：Ctrl+C / kill 等正常退出路径，与崩溃捕获无关，始终注册
const QList<int> kTermSignals = {SIGINT, SIGTERM};

}  // namespace

// 当前实际注册的信号列表（init 时填充，shutdown 时清空）
QList<int> GlobalExceptionHandler::s_signals_;

QList<void (*)(int)> GlobalExceptionHandler::s_oldHandlers_;

GlobalExceptionHandler& GlobalExceptionHandler::instance() {
  static GlobalExceptionHandler handler;
  return handler;
}

GlobalExceptionHandler::GlobalExceptionHandler() = default;

GlobalExceptionHandler::~GlobalExceptionHandler() {
  if (initialized_) {
    shutdown();
  }
}

void GlobalExceptionHandler::init() {
  if (initialized_) {
    return;
  }

  // 后端运行时配置（crash/backend）；非法值按非 builtin 处理，与
  // crash::Init 的回落一致
  const QString backend = ConfigManager::instance().get<QString>(
      QString::fromLatin1(kConfigCrashBackend),
      QString::fromLatin1(kCrashBackendCrashpad));
  const bool enable_crash_signals =
      backend == QLatin1String(kCrashBackendBuiltin);

  s_signals_ = kTermSignals;
  if (enable_crash_signals) {
    s_signals_.append(kCrashSignals);
  }

  setupSignalHandlers();
  qInstallMessageHandler(qtMessageHandler);
  initialized_ = true;

  LOG_INFO("EXCEPTION", "全局异常处理器初始化完成（崩溃信号: {}）",
           enable_crash_signals ? "本模块接管"
                                : "不接管（由崩溃捕获后端或使用者的方案负责）");
}

void GlobalExceptionHandler::shutdown() {
  if (!initialized_) {
    return;
  }

  qInstallMessageHandler(nullptr);
  restoreSignalHandlers();
  initialized_ = false;

  LOG_INFO("EXCEPTION", "全局异常处理器已关闭");
}

void GlobalExceptionHandler::setupSignalHandlers() {
  s_oldHandlers_.clear();
  for (int sig : s_signals_) {
    s_oldHandlers_.append(std::signal(sig, signalHandler));
  }
}

void GlobalExceptionHandler::restoreSignalHandlers() {
  for (int i = 0; i < s_signals_.size() && i < s_oldHandlers_.size(); ++i) {
    std::signal(s_signals_[i], s_oldHandlers_[i]);
  }
  s_oldHandlers_.clear();
}

void GlobalExceptionHandler::signalHandler(int signal) {
  const char* name = "UNKNOWN";
  switch (signal) {
    case SIGABRT:
      name = "SIGABRT";
      break;
    case SIGFPE:
      name = "SIGFPE";
      break;
    case SIGILL:
      name = "SIGILL";
      break;
    case SIGSEGV:
      name = "SIGSEGV";
      break;
    case SIGINT:
      name = "SIGINT";
      break;
    case SIGTERM:
      name = "SIGTERM";
      break;
#ifndef Q_OS_WIN
    case SIGBUS:
      name = "SIGBUS";
      break;
#endif
  }

  QString stackTrace;

#ifdef Q_OS_WIN
  void* stack[32];
  USHORT frames = CaptureStackBackTrace(0, 32, stack, nullptr);
  stackTrace = QString("Signal: %1 (%2), backtrace frames: %3")
                   .arg(name)
                   .arg(signal)
                   .arg(frames);
#else
  void* buffer[32];
  int frames = backtrace(buffer, 32);
  stackTrace = QString("Signal: %1 (%2), backtrace frames: %3")
                   .arg(name)
                   .arg(signal)
                   .arg(frames);
#endif

  LOG_FATAL("EXCEPTION", "捕获信号: {} ({}), 栈帧数: {}", name, signal,
            stackTrace.count('\n') + 1);

  if (auto logger = spdlog::default_logger()) {
    logger->flush();
  }

  emit instance().exceptionCaught(
      QString("Signal: %1 (%2)").arg(name).arg(signal), stackTrace);

  // 复位成系统默认处置再重发。不复位的话 raise() 会再次进入本函数——处理器
  // 递归进自己，进程永远退不出，日志还会被刷爆（实测 10 分钟 269MB，WSLg 里
  // 留下一堆关不掉的窗口，即挂起事项 #1）。这条路径此前只在崩溃信号上验过，
  // 那种情况下进程本就无法继续，问题被掩盖；SIGTERM 这类正常终止信号才把它
  // 暴露出来。signal() 在信号上下文里是安全调用
  std::signal(signal, SIG_DFL);
  std::raise(signal);
}

void GlobalExceptionHandler::qtMessageHandler(QtMsgType type,
                                              const QMessageLogContext& context,
                                              const QString& message) {
  const char* module = context.category ? context.category : "QT";

  switch (type) {
    case QtDebugMsg:
      LOG_DEBUG(module, "{}", message.toStdString());
      break;
    case QtInfoMsg:
      LOG_INFO(module, "{}", message.toStdString());
      break;
    case QtWarningMsg:
      LOG_WARN(module, "{}", message.toStdString());
      break;
    case QtCriticalMsg:
      LOG_ERROR(module, "{}", message.toStdString());
      break;
    case QtFatalMsg:
      LOG_FATAL(module, "{}", message.toStdString());
      if (auto logger = spdlog::default_logger()) {
        logger->flush();
      }
      Logger::shutdown();
      std::abort();
  }
}