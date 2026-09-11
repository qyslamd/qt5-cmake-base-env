#include "log_pattern_builder.h"

#include <QStringList>

#include "config/config_defs.h"
#include "config/config_manager.h"

QString BuildLogPattern(const LogPatternOptions& options) {
  QStringList parts;

  if (options.time) {
    parts << QStringLiteral("[%Y-%m-%d %H:%M:%S.%e]");
  }

  if (options.level) {
    parts << (options.colored_level ? QStringLiteral("[%^%l%$]")
                                    : QStringLiteral("[%l]"));
  }

  if (options.thread) {
    parts << QStringLiteral("[%t]");
  }

  if (options.module) {
    parts << QStringLiteral("[%n]");
  }

  // 源文件与函数名是两个独立开关，各自占一段，可同时打开
  if (options.source) {
    parts << QStringLiteral("[%s:%#]");
  }
  if (options.func) {
    parts << QStringLiteral("[%!]");
  }

  if (options.message) {
    parts << QStringLiteral("%v");
  }

  return parts.join(QLatin1Char(' '));
}

LogPatternOptions LoadLogPatternOptionsFromConfig(bool for_console_sink) {
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
