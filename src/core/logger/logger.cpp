#include "logger.h"

#include <cstddef>
#include <memory>

#include <QCoreApplication>
#include <QObject>
#include <QStandardPaths>

#include "config/config_defs.h"
#include "config/config_manager.h"
#include "log_history_buffer.h"
#include "qt_console_sink.h"

namespace {

// Qt 桥持有的两个 sink。它们的回收顺序见 shutdown() 的注释
std::shared_ptr<QtConsoleSink> s_qt_sink;
LogHistoryBuffer* s_history_buffer = nullptr;

// 配置里的级别是 int（0=debug … 4=fatal），映射表留在 Qt 侧——
// 核心只认强类型的 LogLevel，不认识配置格式
LogLevel LevelFromConfig(int level) {
  switch (level) {
    case 0:
      return LogLevel::kDebug;
    case 2:
      return LogLevel::kWarn;
    case 3:
      return LogLevel::kError;
    case 4:
      return LogLevel::kFatal;
    default:
      return LogLevel::kInfo;
  }
}

}  // namespace

LogPatternOptions Logger::LoadPatternOptions(bool for_console_sink) {
  auto& cfg = ConfigManager::instance();

  LogPatternOptions options;
  options.time = cfg.get<bool>(QString::fromLatin1(kConfigLogFmtTime),
                               kConfigLogFmtDefaultTime);
  options.level = cfg.get<bool>(QString::fromLatin1(kConfigLogFmtLevel),
                                kConfigLogFmtDefaultLevel);
  options.thread = cfg.get<bool>(QString::fromLatin1(kConfigLogFmtThread),
                                 kConfigLogFmtDefaultThread);
  options.module = cfg.get<bool>(QString::fromLatin1(kConfigLogFmtModule),
                                 kConfigLogFmtDefaultModule);
  options.source = cfg.get<bool>(QString::fromLatin1(kConfigLogFmtSource),
                                 kConfigLogFmtDefaultSource);
  options.func = cfg.get<bool>(QString::fromLatin1(kConfigLogFmtFunc),
                               kConfigLogFmtDefaultFunc);
  options.message = cfg.get<bool>(QString::fromLatin1(kConfigLogFmtMessage),
                                  kConfigLogFmtDefaultMessage);
  options.colored_level = for_console_sink;
  return options;
}

LogInitOptions Logger::LoadOptions() {
  LogInitOptions options;

  // 日志目录：AppData/Local/<Org>/<App>/logs。
  // **路径只在这里算**——核心不做平台路径推导，免得它算出来的目录与 Qt 的
  // AppLocalDataLocation 差一点，日志悄悄搬到别处去
  const QString local_path =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  options.log_dir = (local_path + QStringLiteral("/logs")).toStdString();

  QString app_name = QCoreApplication::applicationName();
  if (app_name.isEmpty()) {
    app_name = QStringLiteral("app");
  }
  options.file_name = app_name.toStdString();
  options.pid = static_cast<int>(QCoreApplication::applicationPid());

  auto& cfg = ConfigManager::instance();
  options.max_file_size = static_cast<std::size_t>(cfg.get<int>(
      QString::fromLatin1(kConfigLogMaxFileSize),
      kConfigLogDefaultMaxFileSize));
  options.max_file_count = static_cast<std::size_t>(cfg.get<int>(
      QString::fromLatin1(kConfigLogMaxFileCount),
      kConfigLogDefaultMaxFileCount));
  options.keep_days = cfg.get<int>(QString::fromLatin1(kConfigLogKeepDays),
                                   kConfigLogDefaultKeepDays);
  options.level = LevelFromConfig(cfg.get<int>(
      QString::fromLatin1(kConfigLogLevel), kConfigLogDefaultLevel));
  options.console_pattern = LoadPatternOptions(/*for_console_sink=*/true);
  options.file_pattern = LoadPatternOptions(/*for_console_sink=*/false);
  return options;
}

void Logger::init() {
  if (s_history_buffer != nullptr) {
    return;  // 已经初始化过
  }

  // history buffer 必须先于 QtConsoleSink 创建：sink 的构造函数就拿着它的
  // 指针，第一条日志起就要能 push 进去
  s_history_buffer = new LogHistoryBuffer(5000);
  s_qt_sink = std::make_shared<QtConsoleSink>(s_history_buffer);
  s_qt_sink->set_level(spdlog::level::debug);

  LogCore::Init(LoadOptions(), {s_qt_sink});

  // 监听配置变更，日志级别与格式实时生效。
  // 核心不认识 ConfigManager，所以订阅放在这里，再转成核心的 ApplyOptions
  QObject::connect(
      &ConfigManager::instance(), &ConfigManager::configChanged,
      [](const QString& key) {
        const bool level_changed =
            (key == QString::fromLatin1(kConfigLogLevel));
        const bool format_changed =
            key.startsWith(QString::fromLatin1(kConfigLogFmtPrefix));
        if (!level_changed && !format_changed) {
          return;
        }
        const LogInitOptions options = Logger::LoadOptions();
        LogCore::ApplyOptions(options);
        if (level_changed) {
          LOG_INFO("LOGGER", "日志级别已更新为: {}",
                   static_cast<int>(options.level));
        } else {
          LOG_INFO("LOGGER", "日志格式已更新 [key={}]", key.toStdString());
        }
      });
}

void Logger::shutdown() {
  LogCore::Shutdown();
  // 顺序要紧：核心先放掉它那份 sink 引用，这里再放掉桥的这份，QtConsoleSink
  // 才会真的析构。反过来的话，delete history buffer 之后 sink 还活着，
  // 它里面的 history_ 就成了悬垂指针
  s_qt_sink.reset();
  delete s_history_buffer;
  s_history_buffer = nullptr;
}

QtConsoleSink* Logger::qtConsoleSink() { return s_qt_sink.get(); }

LogHistoryBuffer* Logger::qtHistoryBuffer() { return s_history_buffer; }
