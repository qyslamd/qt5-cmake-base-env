#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include <QCoreApplication>
#include <QList>
#include <QObject>
#include <QString>

#include "logger/log_history_buffer.h"
#include "logger/logger.h"
#include "logger/qt_console_sink.h"

// 日志的启动期行为。
//
// 需要 QCoreApplication（Logger::init 要从 applicationName 推日志目录、要读配置），
// 所以自带 main，不链 gtest_main。

namespace {

// 异步 logger 走 spdlog 工作线程，消息要等一拍才落地
constexpr int kPollTimes = 200;
constexpr int kPollIntervalMs = 10;

QString JoinLines(const QList<LogEntry>& entries) {
  QString all;
  for (const LogEntry& entry : entries) {
    all += entry.text;
    all += QLatin1Char('\n');
  }
  return all;
}

}  // namespace

TEST(LoggerTest, PreInitLogsSurviveInit) {
  // init 之前打的两条。以前 getLogger 返回 nullptr，宏体里的 if (_l) 把它们
  // 直接丢掉——而"是否已有实例"恰恰是这段时间里最要紧的线索
  LOG_INFO("MAIN", "启动期哨兵 pre-init-sentinel");
  LOG_WARN("SingleInstance", "启动期哨兵 warn-sentinel");

  Logger::init();

  // 启动期的历史日志走 LogHistoryBuffer 回放，注意它是**一次性**的
  // （drained_ 闩锁），drain 第二次就成了 noop
  LogHistoryBuffer* history = Logger::qtHistoryBuffer();
  ASSERT_NE(nullptr, history);
  QList<LogEntry> collected;
  QObject context;
  QObject::connect(
      history, &LogHistoryBuffer::drained, &context,
      [&collected](const QList<LogEntry>& entries) {
        collected.append(entries);
      });
  for (int i = 0; i < kPollTimes && collected.size() < 3; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
    history->drain(&context);
  }

  const QString all = JoinLines(collected);
  EXPECT_TRUE(all.contains(QStringLiteral("pre-init-sentinel")))
      << all.toStdString();
  EXPECT_TRUE(all.contains(QStringLiteral("warn-sentinel")))
      << all.toStdString();
  // 回放的行时间戳早于"日志系统初始化完成"，不标出来会让人以为时间倒流
  EXPECT_TRUE(all.contains(QStringLiteral("[启动期]"))) << all.toStdString();
}

TEST(LoggerTest, PostInitLoggingStillWorks) {
  // 回归：改了 getLogger 的分支之后，正常路径照旧。
  // 实时日志走 QtConsoleSink 的信号（history 已经 drain 过了）
  QtConsoleSink* sink = Logger::qtConsoleSink();
  ASSERT_NE(nullptr, sink);

  QStringList lines;
  QObject context;
  QObject::connect(sink, &QtConsoleSink::logMessage, &context,
                   [&lines](int, const QString& text) { lines.append(text); });

  LOG_INFO("MAIN", "init 之后 post-init-sentinel");
  // sink 在 spdlog 工作线程里 emit，跨线程投递要靠事件循环送达
  for (int i = 0; i < kPollTimes && lines.isEmpty(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
    QCoreApplication::processEvents();
  }

  ASSERT_FALSE(lines.isEmpty()) << "init 之后的日志没有到达界面 sink";
  EXPECT_TRUE(lines.join(QString()).contains(QStringLiteral("post-init-sentinel")));
}

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  // 用独立的 App 名，日志与配置落在自己的目录里，不碰真实程序的
  QCoreApplication::setOrganizationName(QStringLiteral("ephad_test"));
  QCoreApplication::setApplicationName(QStringLiteral("ephad_logger_test"));

  ::testing::InitGoogleTest(&argc, argv);
  const int result = RUN_ALL_TESTS();

  Logger::shutdown();
  return result;
}
