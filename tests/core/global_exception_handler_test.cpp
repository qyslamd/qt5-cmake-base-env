#include <gtest/gtest.h>

#include <csignal>
#include <cstdlib>

#include <QCoreApplication>
#include <QProcess>
#include <QString>

#include "crashhandler/global_exception_handler.h"

// GEH 对 SIGINT/SIGTERM 的处理：**收到之后进程必须真的退出**。
//
// 这条用例是挂起事项 #1（"SIGTERM 杀不掉应用"）的回归。原先 signalHandler
// 在末尾直接 std::raise(signal) 重发，而处理函数没复位——raise 于是又进了
// 自己，递归到栈溢出为止：进程永远不退，日志还被刷爆（实测 10 分钟 269MB，
// WSLg 里留下一堆关不掉的窗口）。修法是重发前先把处置复位成 SIG_DFL。
//
// 为什么要子进程：信号处置是进程级的，主进程里 raise 一下自己就没了；沿用
// crashhandler_test 那套"父进程拉起自己、参数区分角色"的骨架。
//
// 判别力在 Linux（POSIX）一侧：MSVC 的 CRT 在处理器返回后会终止进程，所以
// Windows 上不复位也照样退出、这条用例恒过；POSIX 的处理函数是常驻的，
// 不复位就必然重入自己——挂起事项 #1 正是在 WSL 上暴露的。
namespace {

constexpr const char* kTermChildArg = "--geh-term-child=";

// 子进程应在多久内死掉。这个超时**就是判据**：修复前它永远不死。给得比
// ctest 的用例超时小得多，卡住时看到的是断言失败，而不是一句 timeout
constexpr int kTermTimeoutMs = 10000;

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

// 子进程入口：装好 GEH 后给自己发一个终止信号。正常情况下不会返回
[[noreturn]] void RunTermChild(const QString& signal_name) {
  GlobalExceptionHandler::instance().init();
  if (signal_name == QStringLiteral("sigterm")) {
    std::raise(SIGTERM);
  } else if (signal_name == QStringLiteral("sigint")) {
    std::raise(SIGINT);
  }
  // 走到这里说明信号没把进程带走
  std::_Exit(99);
}

// 拉起子进程发一次信号，返回它是否在超时内退出
bool TerminatesWithin(const QString& signal_name) {
  QProcess child;
  child.setProgram(QCoreApplication::applicationFilePath());
  child.setArguments({QString::fromLatin1(kTermChildArg) + signal_name});
  child.start();
  if (!child.waitForStarted(5000)) {
    return false;
  }
  const bool finished = child.waitForFinished(kTermTimeoutMs);
  if (!finished) {
    child.kill();
    child.waitForFinished(5000);
  }
  return finished;
}

}  // namespace

TEST(GlobalExceptionHandlerTest, SigtermTerminatesTheProcess) {
  EXPECT_TRUE(TerminatesWithin(QStringLiteral("sigterm")))
      << "SIGTERM 之后进程没有退出：信号处理器很可能又递归进了自己"
         "（见本文件头部的挂起事项 #1）";
}

TEST(GlobalExceptionHandlerTest, SigintTerminatesTheProcess) {
  EXPECT_TRUE(TerminatesWithin(QStringLiteral("sigint")))
      << "SIGINT（Ctrl+C）之后进程没有退出";
}

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);

  // 子进程角色须在 gtest 初始化之前拦截
  const QString term_signal = FindArgValue(argc, argv, kTermChildArg);
  if (!term_signal.isEmpty()) {
    RunTermChild(term_signal);
  }

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
