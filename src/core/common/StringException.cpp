#include "common/StringException.h"

#include <QString>

StringException::StringException(Code code, const QString& message)
    : std::runtime_error(message.toStdString()),
      code_(code),
      input_(),
      message_(message) {}

StringException::StringException(Code code, const QString& message,
                                 const QString& input)
    : std::runtime_error(message.toStdString()),
      code_(code),
      input_(input),
      message_(message) {}

StringException::Code StringException::code() const { return code_; }

const QString& StringException::input() const { return input_; }

const QString& StringException::message() const { return message_; }

QString StringException::toString() const {
  QString result = QString("StringException[%1]: %2")
                       .arg(QString::fromStdString(std::runtime_error::what()))
                       .arg(message_);
  if (!input_.isEmpty()) {
    result += QString(" (input: %1)").arg(input_);
  }
  return result;
}
