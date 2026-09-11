#include "common/FileException.h"

#include <QString>

FileException::FileException(Code code, const QString& message)
    : std::runtime_error(message.toStdString()),
      code_(code),
      path_(),
      message_(message) {}

FileException::FileException(Code code,
                             const QString& message,
                             const QString& path)
    : std::runtime_error(message.toStdString()),
      code_(code),
      path_(path),
      message_(message) {}

FileException::Code FileException::code() const {
  return code_;
}

const QString& FileException::path() const {
  return path_;
}

const QString& FileException::message() const {
  return message_;
}

QString FileException::toString() const {
  QString result = QString("FileException[%1]: %2")
                       .arg(QString::fromStdString(std::runtime_error::what()))
                       .arg(message_);
  if (!path_.isEmpty()) {
    result += QString(" (path: %1)").arg(path_);
  }
  return result;
}