#include <QtGlobal>
#ifdef Q_OS_WIN

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include "WindowsCrashHandler.h"


#pragma comment(lib, "dbghelp.lib")

WindowsCrashHandler* WindowsCrashHandler::s_instance = nullptr;

WindowsCrashHandler::WindowsCrashHandler() : m_prevFilter(nullptr) {
  s_instance = this;
  // 默认崩溃日志路径：AppData/Local/<AppName>/crash/
  QString localPath =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  m_dumpPath = localPath + "/crash/";
  QDir().mkpath(m_dumpPath);
}

WindowsCrashHandler::~WindowsCrashHandler() {
  if (m_vectoredHandler) {
    RemoveVectoredExceptionHandler(m_vectoredHandler);
    m_vectoredHandler = nullptr;
  }
  if (m_prevFilter) {
    SetUnhandledExceptionFilter(m_prevFilter);
  }
  s_instance = nullptr;
}

bool WindowsCrashHandler::init() {
  m_prevFilter = SetUnhandledExceptionFilter(exceptionHandler);
  m_vectoredHandler = AddVectoredExceptionHandler(1, vectoredHandler);
  return m_prevFilter != nullptr || m_vectoredHandler != nullptr;
}

void WindowsCrashHandler::setDumpPath(const QString& path) {
  m_dumpPath = path;
  QDir().mkpath(m_dumpPath);
}

void WindowsCrashHandler::setCrashCallback(
    std::function<void(const QString& crashLog)> callback) {
  m_crashCallback = std::move(callback);
}

LONG WINAPI
WindowsCrashHandler::vectoredHandler(PEXCEPTION_POINTERS pExceptionInfo) {
  if (!s_instance) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;
  // 只处理真正的崩溃异常，忽略调试和控制流异常
  if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_STACK_OVERFLOW ||
      code == EXCEPTION_ILLEGAL_INSTRUCTION ||
      code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
      code == EXCEPTION_FLT_DIVIDE_BY_ZERO || code == EXCEPTION_IN_PAGE_ERROR ||
      code == EXCEPTION_ARRAY_BOUNDS_EXCEEDED ||
      code == EXCEPTION_DATATYPE_MISALIGNMENT ||
      code == EXCEPTION_PRIV_INSTRUCTION ||
      code == EXCEPTION_NONCONTINUABLE_EXCEPTION) {
    return exceptionHandler(pExceptionInfo);
  }

  return EXCEPTION_CONTINUE_SEARCH;
}

// ── Helper: crash handler body ──
void WindowsCrashHandler::doCrashHandler(PEXCEPTION_POINTERS pExceptionInfo) {
  // 收集崩溃信息
  QString crashLog;
  crashLog += "=== 崩溃信息 ===\n";
  crashLog += QString("异常代码：%1 (%2)\n")
                  .arg(pExceptionInfo->ExceptionRecord->ExceptionCode, 8, 16,
                       QChar('0'))
                  .arg(getExceptionString(
                      pExceptionInfo->ExceptionRecord->ExceptionCode));
  crashLog += QString("异常地址：0x%1\n")
                  .arg(reinterpret_cast<quintptr>(
                           pExceptionInfo->ExceptionRecord->ExceptionAddress),
                       QT_POINTER_SIZE * 2, 16, QChar('0'));

  crashLog += "\n=== 寄存器上下文 ===\n";
  crashLog += getRegisterContext(pExceptionInfo->ContextRecord);

  crashLog += "\n=== 调用栈 ===\n";
  crashLog += getCallStack(pExceptionInfo->ContextRecord);

  crashLog += s_instance->collectCommonInfo();

  // 写入日志文件
  QString fileName = s_instance->generateCrashFileName();
  QString fullPath = s_instance->m_dumpPath + fileName;

  QFile file(fullPath);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    file.write(crashLog.toUtf8());
    file.close();
  }

  // 写入MiniDump文件
  QString dumpFileName = fileName;
  dumpFileName.replace(".log", ".dmp");
  QString dumpFullPath = s_instance->m_dumpPath + dumpFileName;

  HANDLE hDumpFile = CreateFileW(
      reinterpret_cast<LPCWSTR>(dumpFullPath.utf16()), GENERIC_WRITE, 0,
      nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (hDumpFile != INVALID_HANDLE_VALUE) {
    MINIDUMP_EXCEPTION_INFORMATION mei;
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = pExceptionInfo;
    mei.ClientPointers = FALSE;

    MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
        MiniDumpWithDataSegs | MiniDumpWithHandleData |
        MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo);

    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile,
                      dumpType, &mei, nullptr, nullptr);
    CloseHandle(hDumpFile);
  }

  // 执行回调
  if (s_instance->m_crashCallback) {
    s_instance->m_crashCallback(crashLog);
  }

  // 显示友好提示（仅当QApplication存在时）
  if (QCoreApplication::instance()) {
    auto* app = QCoreApplication::instance();
    if (app->inherits("QApplication")) {
      std::wstring msg = L"程序发生异常，已生成崩溃日志：\n" +
                         fullPath.toStdWString() + L"\n已生成转储文件：\n" +
                         dumpFullPath.toStdWString() +
                         L"\n\n请联系开发者解决问题。";
      MessageBoxW(nullptr, msg.c_str(), L"程序崩溃", MB_OK | MB_ICONERROR);
    }
  }
}

// ── Helper: fallback handler (no heap allocation, only stack buffers) ──
void WindowsCrashHandler::doCrashFallback(PEXCEPTION_POINTERS pExceptionInfo) {
  wchar_t fallbackWide[MAX_PATH] = {0};
  if (s_instance) {
    ::wcsncpy_s(
        fallbackWide, MAX_PATH,
        reinterpret_cast<const wchar_t*>(s_instance->m_dumpPath.utf16()),
        _TRUNCATE);
  }
  if (fallbackWide[0] == L'\0') {
    ::wcsncpy_s(fallbackWide, MAX_PATH, L".\\crash\\", _TRUNCATE);
  }
  ::CreateDirectoryW(fallbackWide, nullptr);

  wchar_t logPathW[MAX_PATH];
  ::swprintf_s(logPathW, MAX_PATH, L"%scrash_fallback_%llu.log", fallbackWide,
               (unsigned long long)::GetTickCount64());

  HANDLE hLog = ::CreateFileW(logPathW, GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hLog != INVALID_HANDLE_VALUE) {
    const char msg[] = "[嵌套异常] 崩溃处理程序内部发生异常，可能是堆损坏\n";
    DWORD written = 0;
    ::WriteFile(hLog, msg, (DWORD)strlen(msg), &written, nullptr);

    char addrStr[128];
    ::snprintf(addrStr, sizeof(addrStr), "原始异常地址: 0x%p\n",
               pExceptionInfo->ExceptionRecord->ExceptionAddress);
    ::WriteFile(hLog, addrStr, (DWORD)strlen(addrStr), &written, nullptr);
    ::CloseHandle(hLog);
  }
}

// ── Helper: stack walk loop (try/catch with /EHa traps SEH) ──
void WindowsCrashHandler::appendStackWalk(QString& out,
                                          HANDLE process,
                                          HANDLE thread,
                                          CONTEXT* ctxCopy,
                                          STACKFRAME64& stackFrame,
                                          DWORD machineType) {
  char symbolBuffer[sizeof(SYMBOL_INFO) + 256] = {0};
  PSYMBOL_INFO pSymbol = reinterpret_cast<PSYMBOL_INFO>(symbolBuffer);
  pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  pSymbol->MaxNameLen = 255;

  IMAGEHLP_LINE64 lineInfo = {0};
  lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

  try {
    for (int i = 0; i < 64; ++i) {
      if (!StackWalk64(machineType, process, thread, &stackFrame, ctxCopy,
                       nullptr, SymFunctionTableAccess64, SymGetModuleBase64,
                       nullptr)) {
        break;
      }

      if (stackFrame.AddrPC.Offset == 0) {
        break;
      }

      DWORD64 displacement = 0;
      if (SymFromAddr(process, stackFrame.AddrPC.Offset, &displacement,
                      pSymbol)) {
        out += QString("%1: 0x%2 %3()")
                   .arg(i, 2)
                   .arg(stackFrame.AddrPC.Offset, QT_POINTER_SIZE * 2, 16,
                        QChar('0'))
                   .arg(pSymbol->Name);

        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(process, stackFrame.AddrPC.Offset,
                                 &lineDisplacement, &lineInfo)) {
          out +=
              QString(" %1:%2").arg(lineInfo.FileName).arg(lineInfo.LineNumber);
        }
        out += "\n";
      } else {
        out += QString("%1: 0x%2 [未知函数]\n")
                   .arg(i, 2)
                   .arg(stackFrame.AddrPC.Offset, QT_POINTER_SIZE * 2, 16,
                        QChar('0'));
      }
    }
  } catch (...) {
    out += QString("[栈回溯因栈帧损坏而中断]\n");
  }
}

LONG WINAPI
WindowsCrashHandler::exceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
  if (!s_instance) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  try {
    doCrashHandler(pExceptionInfo);
  } catch (...) {
    doCrashFallback(pExceptionInfo);
  }

  // 终止程序
  TerminateProcess(GetCurrentProcess(),
                   pExceptionInfo->ExceptionRecord->ExceptionCode);
  return EXCEPTION_EXECUTE_HANDLER;
}

QString WindowsCrashHandler::getExceptionString(DWORD exceptionCode) {
  switch (exceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
      return "内存访问违规";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
      return "数组越界";
    case EXCEPTION_BREAKPOINT:
      return "断点触发";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
      return "数据类型未对齐";
    case EXCEPTION_FLT_DENORMAL_OPERAND:
      return "浮点操作数异常";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
      return "浮点除零";
    case EXCEPTION_FLT_INEXACT_RESULT:
      return "浮点结果不精确";
    case EXCEPTION_FLT_INVALID_OPERATION:
      return "无效浮点操作";
    case EXCEPTION_FLT_OVERFLOW:
      return "浮点溢出";
    case EXCEPTION_FLT_STACK_CHECK:
      return "浮点栈检查失败";
    case EXCEPTION_FLT_UNDERFLOW:
      return "浮点下溢";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
      return "非法指令";
    case EXCEPTION_IN_PAGE_ERROR:
      return "页错误";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
      return "整数除零";
    case EXCEPTION_INT_OVERFLOW:
      return "整数溢出";
    case EXCEPTION_INVALID_DISPOSITION:
      return "无效处置";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
      return "不可继续异常";
    case EXCEPTION_PRIV_INSTRUCTION:
      return "特权指令";
    case EXCEPTION_SINGLE_STEP:
      return "单步执行";
    case EXCEPTION_STACK_OVERFLOW:
      return "栈溢出";
    default:
      return "未知异常";
  }
}

QString WindowsCrashHandler::getRegisterContext(CONTEXT* context) {
  QString regs;
#ifdef _WIN64
  regs += QString("RAX: 0x%1\n").arg(context->Rax, 16, 16, QChar('0'));
  regs += QString("RBX: 0x%1\n").arg(context->Rbx, 16, 16, QChar('0'));
  regs += QString("RCX: 0x%1\n").arg(context->Rcx, 16, 16, QChar('0'));
  regs += QString("RDX: 0x%1\n").arg(context->Rdx, 16, 16, QChar('0'));
  regs += QString("RSP: 0x%1\n").arg(context->Rsp, 16, 16, QChar('0'));
  regs += QString("RBP: 0x%1\n").arg(context->Rbp, 16, 16, QChar('0'));
  regs += QString("RSI: 0x%1\n").arg(context->Rsi, 16, 16, QChar('0'));
  regs += QString("RDI: 0x%1\n").arg(context->Rdi, 16, 16, QChar('0'));
  regs += QString("RIP: 0x%1\n").arg(context->Rip, 16, 16, QChar('0'));
#else
  regs += QString("EAX: 0x%1\n").arg(context->Eax, 8, 16, QChar('0'));
  regs += QString("EBX: 0x%1\n").arg(context->Ebx, 8, 16, QChar('0'));
  regs += QString("ECX: 0x%1\n").arg(context->Ecx, 8, 16, QChar('0'));
  regs += QString("EDX: 0x%1\n").arg(context->Edx, 8, 16, QChar('0'));
  regs += QString("ESP: 0x%1\n").arg(context->Esp, 8, 16, QChar('0'));
  regs += QString("EBP: 0x%1\n").arg(context->Ebp, 8, 16, QChar('0'));
  regs += QString("ESI: 0x%1\n").arg(context->Esi, 8, 16, QChar('0'));
  regs += QString("EDI: 0x%1\n").arg(context->Edi, 8, 16, QChar('0'));
  regs += QString("EIP: 0x%1\n").arg(context->Eip, 8, 16, QChar('0'));
#endif
  return regs;
}

QString WindowsCrashHandler::getCallStack(CONTEXT* context) {
  // StackWalk64 modifies the CONTEXT in-place. When the stack is corrupted
  // (common in AV crashes), it may trigger another AV. Work on a copy and
  // delegate the walk to appendStackWalk which has SEH protection.
  CONTEXT ctxCopy;
  memcpy(&ctxCopy, context, sizeof(ctxCopy));

  QString stack;
  HANDLE process = GetCurrentProcess();
  HANDLE thread = GetCurrentThread();

  SymInitialize(process, nullptr, TRUE);
  SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

  STACKFRAME64 stackFrame = {0};
#ifdef _WIN64
  stackFrame.AddrPC.Offset = ctxCopy.Rip;
  stackFrame.AddrPC.Mode = AddrModeFlat;
  stackFrame.AddrFrame.Offset = ctxCopy.Rbp;
  stackFrame.AddrFrame.Mode = AddrModeFlat;
  stackFrame.AddrStack.Offset = ctxCopy.Rsp;
  stackFrame.AddrStack.Mode = AddrModeFlat;
  DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
#else
  stackFrame.AddrPC.Offset = ctxCopy.Eip;
  stackFrame.AddrPC.Mode = AddrModeFlat;
  stackFrame.AddrFrame.Offset = ctxCopy.Ebp;
  stackFrame.AddrFrame.Mode = AddrModeFlat;
  stackFrame.AddrStack.Offset = ctxCopy.Esp;
  stackFrame.AddrStack.Mode = AddrModeFlat;
  DWORD machineType = IMAGE_FILE_MACHINE_I386;
#endif

  appendStackWalk(stack, process, thread, &ctxCopy, stackFrame, machineType);

  SymCleanup(process);
  return stack;
}

#endif  // Q_OS_WIN
