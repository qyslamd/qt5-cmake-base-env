#include <cstdlib>
#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "crashhandler/crash_handler.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

// 子进程模式参数。本测试二进制会在崩溃场景下被自己重新拉起：
// 子进程初始化 CrashHandler 后触发指定异常，父进程据此断言日志落盘。
// 采用 QCoreApplication（而非 QApplication）是刻意的：崩溃处理里只有
// QApplication 存在时才会弹 MessageBox，否则会挂住无人值守的测试。
constexpr const char* kCrashChildArg = "--crash-child=";
constexpr const char* kCrashDumpPathArg = "--crash-dump-path=";

// 父进程等待子进程结束的时限。Windows 侧还要等 minidump 落盘，给足时间；
// Linux 侧只写日志，且刻意低于 ctest 的 10 秒总超时——子进程若卡住，想看到
// 的是断言失败，而不是被 ctest 掐掉后只剩一句 timeout
#ifdef Q_OS_WIN
constexpr int kChildFinishTimeoutMs = 60000;
#else
constexpr int kChildFinishTimeoutMs = 8000;
#endif

// 取形如 "--key=value" 的参数值，不存在时返回空串
QString FindArgValue(int argc, char* argv[], const char* key) {
  const QString prefix = QString::fromLatin1(key);
  for (int i = 1; i < argc; ++i) {
    const QString arg = QString::fromLocal8Bit(argv[i]);
    if (arg.startsWith(prefix)) {
      return arg.mid(prefix.size());
    }
  }
  return QString();
}

// 用全局变量承载空指针/零值，避免编译器按 UB 直接优化掉崩溃语句
volatile int* g_null_ptr = nullptr;
volatile int g_zero_divisor = 0;

// 子进程入口：初始化 handler 后触发崩溃。正常情况下不会返回——Linux 侧
// 信号处理器写完日志即 _exit，Windows 侧由异常处理器结束进程
[[noreturn]] void RunCrashChild(const QString& kind, const QString& dump_path) {
#ifdef Q_OS_WIN
  // 抑制 WER 弹窗与调试器挂接，保证崩溃后进程立即退出
  ::SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
  ::SetErrorMode(::SetErrorMode(0) | SEM_NOGPFAULTERRORBOX);
  ::SetUnhandledExceptionFilter(nullptr);
#endif

  auto handler = CrashHandler::create();
  if (!handler) {
    std::_Exit(98);
  }
  handler->setDumpPath(dump_path);
  handler->init();

  if (kind == QStringLiteral("access-violation")) {
    *g_null_ptr = 42;
  } else if (kind == QStringLiteral("divide-by-zero")) {
    volatile int numerator = 1;
    volatile int result = numerator / g_zero_divisor;
    (void)result;
  } else if (kind == QStringLiteral("abort")) {
    // 只有 Linux 侧用到：SIGABRT 在 handler 里走的是另一条头部分支
    // （不打印故障地址，因为 si_addr 对 SIGABRT 没有意义）
    std::abort();
  }

  // 走到这里说明崩溃没有按预期发生
  std::_Exit(99);
}

// 以子进程身份崩溃一次，返回崩溃日志内容；未生成日志时返回空串
QString CrashInChildProcess(const QString& kind) {
  QTemporaryDir tmp;
  if (!tmp.isValid()) {
    return QString();
  }
  // 刻意不带尾分隔符：setDumpPath 需自行规范化，否则崩溃文件会被写到
  // 该目录的兄弟路径上（曾出现过此缺陷，本测试同时作为其回归测试）
  const QString dump_path = tmp.path() + QStringLiteral("/crash");

  QProcess child;
  child.setProgram(QCoreApplication::applicationFilePath());
  child.setArguments({QString::fromLatin1(kCrashChildArg) + kind,
                      QString::fromLatin1(kCrashDumpPathArg) + dump_path});
  child.start();
  if (!child.waitForStarted(5000)) {
    return QString();
  }
  // 需等待写日志（Windows 侧还有 minidump），给足时间
  if (!child.waitForFinished(kChildFinishTimeoutMs)) {
    child.kill();
    return QString();
  }

  const QDir dir(dump_path);
  if (!dir.exists()) {
    return QString();
  }
  const QStringList logs =
      dir.entryList({QStringLiteral("crash_*.log")}, QDir::Files);
  if (logs.isEmpty()) {
    return QString();
  }

  QFile file(dir.absoluteFilePath(logs.first()));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString();
  }
  return QString::fromUtf8(file.readAll());
}

}  // namespace

// ===========================================================================
// 单元测试：不触发真实崩溃，跨平台
// ===========================================================================

TEST(CrashHandlerTest, CreateInstance) {
  auto handler = CrashHandler::create();
  EXPECT_NE(handler, nullptr);
}

TEST(CrashHandlerTest, DefaultDumpPathExists) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);

  // 构造即建立默认目录：AppLocalDataLocation/crash/
  const QString expected =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
      QStringLiteral("/crash/");
  EXPECT_TRUE(QDir(expected).exists());
}

TEST(CrashHandlerTest, SetCustomDumpPathCreatesDirectory) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);

  QTemporaryDir tmp;
  ASSERT_TRUE(tmp.isValid());
  const QString custom = tmp.path() + QStringLiteral("/custom_crash/");

  handler->setDumpPath(custom);
  EXPECT_TRUE(QDir(custom).exists());
}

TEST(CrashHandlerTest, InitSucceeds) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);
  EXPECT_TRUE(handler->init());
}

TEST(CrashHandlerTest, GenerateFileNameFormat) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);

  const QString name = handler->generateCrashFileName();
  EXPECT_TRUE(name.startsWith(QStringLiteral("crash_")));
  EXPECT_TRUE(name.endsWith(QStringLiteral(".log")));
  // crash_ + yyyyMMdd_HHmmss + .log
  EXPECT_EQ(name.size(), 25);
}

TEST(CrashHandlerTest, CollectCommonInfoContainsKeyFields) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);

  const QString info = handler->collectCommonInfo();
  EXPECT_TRUE(info.contains(QStringLiteral("系统信息")));
  EXPECT_TRUE(info.contains(QStringLiteral("操作系统")));
  EXPECT_TRUE(info.contains(QStringLiteral("程序信息")));
  EXPECT_TRUE(info.contains(QStringLiteral("程序路径")));
  EXPECT_TRUE(info.contains(QStringLiteral("进程ID")));
}

// ===========================================================================
// 集成测试：拉起子进程触发真实崩溃，验证崩溃日志确实落盘
// 两侧各有一组，断言日志内容而非仅文件存在
// ===========================================================================

#ifdef Q_OS_WIN

TEST(WindowsCrashHandlerTest, AccessViolationWritesCrashLog) {
  const QString log = CrashInChildProcess(QStringLiteral("access-violation"));
  ASSERT_FALSE(log.isEmpty()) << "子进程应生成 crash_*.log";

  EXPECT_TRUE(log.contains(QStringLiteral("崩溃信息")));
  EXPECT_TRUE(log.contains(QStringLiteral("内存访问违规")));
  EXPECT_TRUE(log.contains(QStringLiteral("调用栈")));
  EXPECT_TRUE(log.contains(QStringLiteral("操作系统")));
  EXPECT_TRUE(log.contains(QStringLiteral("进程ID")));
}

TEST(WindowsCrashHandlerTest, DivideByZeroWritesCrashLog) {
  const QString log = CrashInChildProcess(QStringLiteral("divide-by-zero"));
  ASSERT_FALSE(log.isEmpty()) << "子进程应生成 crash_*.log";

  EXPECT_TRUE(log.contains(QStringLiteral("崩溃信息")));
  EXPECT_TRUE(log.contains(QStringLiteral("整数除零")));
}

#else

// Linux 侧只断言日志结构与信号名：链接未加 -rdynamic，backtrace 拿不到符号名，
// 只有裸地址。日志里已提示用 addr2line -e <可执行文件> 解析，而 Debug 与
// RelWithDebInfo 的可执行文件带 -g，这条路是通的——故符号名缺失不是缺陷

TEST(LinuxCrashHandlerTest, SegmentationFaultWritesCrashLog) {
  const QString log = CrashInChildProcess(QStringLiteral("access-violation"));
  ASSERT_FALSE(log.isEmpty()) << "子进程应生成 crash_*.log";

  EXPECT_TRUE(log.contains(QStringLiteral("崩溃信息")));
  EXPECT_TRUE(log.contains(QStringLiteral("信号: SIGSEGV")));
  // si_addr 对硬件异常信号有意义，SIGSEGV 应带上故障地址
  EXPECT_TRUE(log.contains(QStringLiteral("故障地址")));
  EXPECT_TRUE(log.contains(QStringLiteral("调用栈")));
  EXPECT_TRUE(log.contains(QStringLiteral("操作系统")));
  EXPECT_TRUE(log.contains(QStringLiteral("进程ID")));
}

TEST(LinuxCrashHandlerTest, DivideByZeroWritesCrashLog) {
  const QString log = CrashInChildProcess(QStringLiteral("divide-by-zero"));
  ASSERT_FALSE(log.isEmpty()) << "子进程应生成 crash_*.log";

  EXPECT_TRUE(log.contains(QStringLiteral("崩溃信息")));
  EXPECT_TRUE(log.contains(QStringLiteral("信号: SIGFPE")));
  EXPECT_TRUE(log.contains(QStringLiteral("调用栈")));
}

TEST(LinuxCrashHandlerTest, AbortWritesCrashLog) {
  const QString log = CrashInChildProcess(QStringLiteral("abort"));
  ASSERT_FALSE(log.isEmpty()) << "子进程应生成 crash_*.log";

  EXPECT_TRUE(log.contains(QStringLiteral("崩溃信息")));
  EXPECT_TRUE(log.contains(QStringLiteral("信号: SIGABRT")));
  EXPECT_TRUE(log.contains(QStringLiteral("调用栈")));
  // SIGABRT 走的是不打印故障地址的那条分支，此处同时是该分支的回归测试
  EXPECT_FALSE(log.contains(QStringLiteral("故障地址")));
}

#endif  // Q_OS_WIN

// ===========================================================================
// 手动崩溃测试：DISABLED_ 前缀使其不参与自动运行，
// 需要人工确认弹窗/日志时用 --gtest_also_run_disabled_tests 单独跑
// ===========================================================================

TEST(CrashHandlerTest, DISABLED_CatchNullPointerCrash) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);
  handler->init();

  int* p = nullptr;
  *p = 1;
}

TEST(CrashHandlerTest, DISABLED_CatchDivideByZeroCrash) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);
  handler->init();

  volatile int a = 1;
  volatile int b = 0;
  volatile int c = a / b;
  (void)c;
}

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);

  // 崩溃子进程模式须在 gtest 初始化之前拦截
  const QString crash_kind = FindArgValue(argc, argv, kCrashChildArg);
  if (!crash_kind.isEmpty()) {
    RunCrashChild(crash_kind, FindArgValue(argc, argv, kCrashDumpPathArg));
  }

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
