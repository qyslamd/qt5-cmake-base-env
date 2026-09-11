#include "common/TimeException.h"

#include <QString>

TimeException::TimeException(Code code, const QString& message)
    : std::runtime_error(message.toStdString()),
      code_(code),
      input_(),
      message_(message) {}

TimeException::TimeException(Code code, const QString& message,
                             const QString& input)
    : std::runtime_error(message.toStdString()),
      code_(code),
      input_(input),
      message_(message) {}

TimeException::Code TimeException::code() const { return code_; }

const QString& TimeException::input() const { return input_; }

const QString& TimeException::message() const { return message_; }

QString TimeException::toString() const {
  QString result = QString("TimeException[%1]: %2")
                       .arg(QString::fromStdString(std::runtime_error::what()))
                       .arg(message_);
  if (!input_.isEmpty()) {
    result += QString(" (input: %1)").arg(input_);
  }
  return result;
}
