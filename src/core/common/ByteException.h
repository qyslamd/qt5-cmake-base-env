#ifndef ETEST_CORE_COMMON_BYTE_EXCEPTION_H_
#define ETEST_CORE_COMMON_BYTE_EXCEPTION_H_

#include <QString>
#include <stdexcept>


class ByteException : public std::runtime_error {
 public:
  enum class Code {
    kInvalidData,
    kSizeMismatch,
    kCrcError,
    kEndianError,
    kUnknown
  };

  explicit ByteException(Code code, const QString& message);
  explicit ByteException(Code code,
                         const QString& message,
                         const QString& detail);

  Code code() const;
  const QString& detail() const;
  const QString& message() const;

  QString toString() const;

 private:
  Code code_;
  QString detail_;
  QString message_;
};

#endif  // ETEST_CORE_COMMON_BYTE_EXCEPTION_H_
