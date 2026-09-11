#ifndef ETEST_CORE_COMMON_TIME_EXCEPTION_H_
#define ETEST_CORE_COMMON_TIME_EXCEPTION_H_

#include <stdexcept>

#include <QString>

class TimeException : public std::runtime_error {
 public:
  enum class Code {
    kInvalidFormat,
    kParseError,
    kOverflow,
    kUnderflow,
    kUnknown
  };

  explicit TimeException(Code code, const QString& message);
  explicit TimeException(Code code, const QString& message,
                         const QString& input);

  Code code() const;
  const QString& input() const;
  const QString& message() const;

  QString toString() const;

 private:
  Code code_;
  QString input_;
  QString message_;
};

#endif  // ETEST_CORE_COMMON_TIME_EXCEPTION_H_