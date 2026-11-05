#include "logger.h"

#include <QTime>

Logger* Logger::Instance() {
  static Logger instance;
  return &instance;
}

void Logger::Log(const QString& msg, LogSource source) {
  const QString timestamped =
      QString("[%1] %2").arg(QTime::currentTime().toString("HH:mm:ss"), msg);
  emit MessageLogged(timestamped, source);
}