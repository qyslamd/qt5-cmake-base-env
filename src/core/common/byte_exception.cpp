#include "common/byte_exception.h"

#include <QString>

ByteException::ByteException(Code code, const QString& message)
    : std::runtime_error(message.toStdString()),
      code_(code),
      detail_(),
      message_(message) {}

ByteException::ByteException(Code code, const QString& message,
                             const QString& detail)
    : std::runtime_error(message.toStdString()),
      code_(code),
      detail_(detail),
      message_(message) {}

ByteException::Code ByteException::code() const { return code_; }

const QString& ByteException::detail() const { return detail_; }

const QString& ByteException::message() const { return message_; }

QString ByteException::toString() const {
  QString result = QString("ByteException[%1]: %2")
                       .arg(QString::fromStdString(std::runtime_error::what()))
                       .arg(message_);
  if (!detail_.isEmpty()) {
    result += QString(" (detail: %1)").arg(detail_);
  }
  return result;
}
