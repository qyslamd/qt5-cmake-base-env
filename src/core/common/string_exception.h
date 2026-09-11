#ifndef FILE_CORE_COMMON_STRING_EXCEPTION_H_
#define FILE_CORE_COMMON_STRING_EXCEPTION_H_

#include <stdexcept>

#include <QString>

class StringException : public std::runtime_error {
 public:
  enum class Code {
    kEncodingError,
    kInvalidFormat,
    kInvalidArgument,
    kOverflow,
    kUnknown
  };

  explicit StringException(Code code, const QString& message);
  explicit StringException(Code code, const QString& message,
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

#endif  // FILE_CORE_COMMON_STRING_EXCEPTION_H_
