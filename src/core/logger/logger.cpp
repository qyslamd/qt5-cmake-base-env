#include "logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QObject>
#include <QStandardPaths>

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
// stdout_color_sink 在 Windows 上映射为 wincolor，其余平台映射为 ansicolor
#include <spdlog/sinks/stdout_color_sinks.h>

#include "config/config_defs.h"
#include "config/config_manager.h"
#include "log_history_buffer.h"
#include "log_pattern_builder.h"
#include "qt_console_sink.h"

namespace {

// 配置日志级别(0..4) 映射到 spdlog 级别
spdlog::level::level_enum LevelFromConfig(int level) {
  switch (level) {
    case 0:
      return spdlog::level::debug;
    case 2:
      return spdlog::level::warn;
    case 3:
      return spdlog::level::err;
    case 4:
      return spdlog::level::critical;
    default:
      return spdlog::level::info;
  }
}

}  // namespace

bool Logger::s_initialized_ = false;
QMutex Logger::s_mutex_;
QtConsoleSink* Logger::s_qt_sink_ = nullptr;
LogHistoryBuffer* Logger::s_history_buffer_ = nullptr;
std::unordered_map<std::string, spdlog::logger*> Logger::s_module_loggers_;
spdlog::sink_ptr Logger::s_console_sink_ = nullptr;
spdlog::sink_ptr Logger::s_file_sink_ = nullptr;

void Logger::ApplyPatternToSinks() {
  // 控制台与界面窗口用彩色级别，文件用非彩色，其余开关一致
  const std::string console_pattern =
      BuildLogPattern(LoadLogPatternOptionsFromConfig(true)).toStdString();
  const std::string file_pattern =
      BuildLogPattern(LoadLogPatternOptionsFromConfig(false)).toStdString();

  if (s_console_sink_) {
    s_console_sink_->set_pattern(console_pattern);
  }
  if (s_qt_sink_) {
    s_qt_sink_->set_pattern(console_pattern);
  }
  if (s_file_sink_) {
    s_file_sink_->set_pattern(file_pattern);
  }
}

void Logger::init() {
  if (s_initialized_) {
    return;
  }

  // 创建日志存储目录：AppData/Local/<AppName>/logs
  QString local_path =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  QString log_dir = local_path + "/logs";
  QDir().mkpath(log_dir);

  // 日志参数从配置读取
  int keep_days = ConfigManager::instance().get<int>(
      QString::fromLatin1(kConfigLogKeepDays), kConfigLogDefaultKeepDays);
  int max_file_size = ConfigManager::instance().get<int>(
      QString::fromLatin1(kConfigLogMaxFileSize), kConfigLogDefaultMaxFileSize);
  int max_file_count = ConfigManager::instance().get<int>(
      QString::fromLatin1(kConfigLogMaxFileCount),
      kConfigLogDefaultMaxFileCount);
  int log_level = ConfigManager::instance().get<int>(
      QString::fromLatin1(kConfigLogLevel), kConfigLogDefaultLevel);

  // 清理历史日志（保留 keep_days 天）
  QDir dir(log_dir);
  dir.setNameFilters({"*.log"});
  dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
  QFileInfoList file_list = dir.entryInfoList();
  QDateTime now = QDateTime::currentDateTime();
  for (const auto& file_info : file_list) {
    if (file_info.lastModified().daysTo(now) > keep_days) {
      QFile::remove(file_info.absoluteFilePath());
    }
  }

  // 异步日志配置
  spdlog::init_thread_pool(8192, 1);

  // 控制台彩色 sink（仅 Debug 构建启用）
#ifndef NDEBUG
  auto console_sink =
      std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::debug);
  // pattern 统一在 sink 建齐后由 ApplyPatternToSinks 按配置设置
  s_console_sink_ = console_sink;
#endif

  // 滚动文件 sink；文件名随 applicationName 走，避免模板产物互相覆盖
  QString app_name = QCoreApplication::applicationName();
  if (app_name.isEmpty()) {
    app_name = QStringLiteral("app");
  }
  QString log_path = log_dir + "/" + app_name + ".log";
  auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      log_path.toStdString(), max_file_size, max_file_count, true);
  file_sink->set_level(spdlog::level::debug);
  s_file_sink_ = file_sink;

  // 启动期历史日志缓冲（环形 5000 条）。UI 日志面板出现后回放。
  // 必须先于 QtConsoleSink 创建，让首条日志即能 push 到 history。
  s_history_buffer_ = new LogHistoryBuffer(5000);

  // Qt 控制台 sink，输出到界面
  auto qt_sink = std::make_shared<QtConsoleSink>(s_history_buffer_);
  qt_sink->set_level(spdlog::level::debug);
  s_qt_sink_ = qt_sink.get();

  // 三个 sink 已建齐，按配置统一设置 pattern
  ApplyPatternToSinks();

  // 注册默认 logger，所有模块共用
#ifndef NDEBUG
  std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink, qt_sink};
#else
  std::vector<spdlog::sink_ptr> sinks = {file_sink, qt_sink};
#endif
  auto default_logger = std::make_shared<spdlog::async_logger>(
      "default", sinks.begin(), sinks.end(), spdlog::thread_pool(),
      spdlog::async_overflow_policy::block);
  default_logger->flush_on(spdlog::level::err);
  spdlog::register_logger(default_logger);
  spdlog::set_default_logger(default_logger);

  // 从配置设置日志级别
  spdlog::set_level(LevelFromConfig(log_level));

  // 自动每秒 flush 一次
  spdlog::flush_every(std::chrono::seconds(1));

  s_initialized_ = true;

  // 预注册常用模块 logger。懒创建在首个模块首次实例化时偶发不稳定
  // （logger 静默丢弃全部输出），提前在单线程初始化阶段一次建好。
  static const char* kPreRegisterModules[] = {
      "ACQUISITION", "ALARM",    "AUTH",    "COMM", "CONFIG",   "CURVE",
      "DASHBOARD",   "LOGIN",    "LOGGER",  "MAIN", "PRESSURE", "PROCESS",
      "REPLAY",      "SELFTEST", "STORAGE", "UI"};
  {
    QMutexLocker locker(&s_mutex_);
    for (const char* name : kPreRegisterModules) {
      getOrCreateModuleLogger(name);
    }
  }

  // 监听配置变更，日志级别实时生效
  QObject::connect(&ConfigManager::instance(), &ConfigManager::configChanged,
                   [](const QString& key) {
                     if (key == QString::fromLatin1(kConfigLogLevel)) {
                       int level = ConfigManager::instance().get<int>(
                           QString::fromLatin1(kConfigLogLevel),
                           kConfigLogDefaultLevel);
                       spdlog::set_level(LevelFromConfig(level));
                       LOG_INFO("LOGGER", "日志级别已更新为: {}", level);
                     }
                     // 格式开关变更即重建全部 sink 的 pattern
                     if (key.startsWith(
                             QString::fromLatin1(kConfigLogFmtPrefix))) {
                       ApplyPatternToSinks();
                       LOG_INFO("LOGGER", "日志格式已更新 [key={}]",
                                key.toStdString());
                     }
                   });

  LOG_INFO("LOGGER", "日志系统初始化完成，日志目录: {}", log_dir.toStdString());
}

void Logger::shutdown() {
  if (!s_initialized_) {
    return;
  }
  LOG_INFO("LOGGER", "日志系统关闭");
  s_module_loggers_.clear();
  s_qt_sink_ = nullptr;
  spdlog::shutdown();
  // spdlog::shutdown() 析构所有 sink（QtConsoleSink 在其中），
  // 之后 history_buffer_ 已无引用者，可以安全 delete。
  delete s_history_buffer_;
  s_history_buffer_ = nullptr;
  s_initialized_ = false;
}

void Logger::setLevel(const QString& module, LogLevel level) {
  if (!s_initialized_) {
    return;
  }
  QMutexLocker locker(&s_mutex_);
  std::string key = module.toStdString();
  auto it = s_module_loggers_.find(key);
  if (it != s_module_loggers_.end()) {
    it->second->set_level(static_cast<spdlog::level::level_enum>(level));
  }
}

void Logger::setAllLevel(LogLevel level) {
  if (!s_initialized_) {
    return;
  }
  spdlog::set_level(static_cast<spdlog::level::level_enum>(level));
  // 同步更新所有模块 logger 的级别
  QMutexLocker locker(&s_mutex_);
  for (auto& pair : s_module_loggers_) {
    pair.second->set_level(static_cast<spdlog::level::level_enum>(level));
  }
}

spdlog::logger* Logger::getOrCreateModuleLogger(
    const std::string& module_name) {
  auto it = s_module_loggers_.find(module_name);
  if (it != s_module_loggers_.end()) {
    return it->second;
  }

  // 按需创建模块 logger，复用 default logger 的 sink
  auto default_logger = spdlog::default_logger();
  if (!default_logger) {
    return nullptr;
  }

  auto module_logger = std::make_shared<spdlog::async_logger>(
      module_name, default_logger->sinks().begin(),
      default_logger->sinks().end(), spdlog::thread_pool(),
      spdlog::async_overflow_policy::block);
  module_logger->set_level(default_logger->level());
  module_logger->flush_on(spdlog::level::err);

  // 如果 spdlog 全局 registry 中已存在同名 logger，先移除再注册
  if (spdlog::get(module_name)) {
    spdlog::drop(module_name);
  }
  spdlog::register_logger(module_logger);

  auto* raw_ptr = module_logger.get();
  s_module_loggers_[module_name] = raw_ptr;
  return raw_ptr;
}

spdlog::logger* Logger::getLogger(const QString& module) {
  if (!s_initialized_) {
    return nullptr;
  }
  QMutexLocker locker(&s_mutex_);
  return getOrCreateModuleLogger(module.toStdString());
}

QtConsoleSink* Logger::qtConsoleSink() {
  return s_qt_sink_;
}

LogHistoryBuffer* Logger::qtHistoryBuffer() {
  return s_history_buffer_;
}
