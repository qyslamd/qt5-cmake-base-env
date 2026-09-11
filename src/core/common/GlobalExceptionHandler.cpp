#include "common/GlobalExceptionHandler.h"

#include <csignal>
#include <cstdlib>

#include <QDebug>

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

QList<int> GlobalExceptionHandler::s_signals_ = {SIGABRT,
                                                 SIGFPE,
                                                 SIGILL,
                                                 SIGSEGV,
                                                 SIGINT,
                                                 SIGTERM
#ifndef Q_OS_WIN
                                                 ,
                                                 SIGBUS
#endif
};

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

  setupSignalHandlers();
  qInstallMessageHandler(qtMessageHandler);
  initialized_ = true;

  LOG_INFO("EXCEPTION", "全局异常处理器初始化完成");
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