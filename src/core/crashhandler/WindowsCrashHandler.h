#include <QtGlobal>
#ifdef Q_OS_WIN

#ifndef ETEST_CORE_CRASHHANDLER_WINDOWSCRASHHANDLER_H_
#define ETEST_CORE_CRASHHANDLER_WINDOWSCRASHHANDLER_H_

#include <dbghelp.h>
#include <windows.h>
#include "CrashHandler.h"


class WindowsCrashHandler : public CrashHandler {
 public:
  WindowsCrashHandler();
  ~WindowsCrashHandler() override;

  bool init() override;
  void setDumpPath(const QString& path) override;
  void setCrashCallback(
      std::function<void(const QString& crashLog)> callback) override;

 private:
  static LONG WINAPI exceptionHandler(PEXCEPTION_POINTERS pExceptionInfo);
  static LONG WINAPI vectoredHandler(PEXCEPTION_POINTERS pExceptionInfo);
  static QString getExceptionString(DWORD exceptionCode);
  static QString getCallStack(CONTEXT* context);
  static QString getRegisterContext(CONTEXT* context);

  // SEH-safe helpers — their local variables must be POD only (no C++ dtors).
  static void doCrashHandler(PEXCEPTION_POINTERS pExceptionInfo);
  static void doCrashFallback(PEXCEPTION_POINTERS pExceptionInfo);
  static void appendStackWalk(QString& out,
                              HANDLE process,
                              HANDLE thread,
                              CONTEXT* ctxCopy,
                              STACKFRAME64& stackFrame,
                              DWORD machineType);

  static WindowsCrashHandler* s_instance;
  QString m_dumpPath;
  std::function<void(const QString&)> m_crashCallback;
  LPTOP_LEVEL_EXCEPTION_FILTER m_prevFilter;
  PVOID m_vectoredHandler = nullptr;
};

#endif  // ETEST_CORE_CRASHHANDLER_WINDOWSCRASHHANDLER_H_

#endif  // Q_OS_WIN
