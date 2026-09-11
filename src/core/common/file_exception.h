#ifndef FILE_CORE_COMMON_FILE_EXCEPTION_H_
#define FILE_CORE_COMMON_FILE_EXCEPTION_H_

#include <stdexcept>

#include <QString>

class FileException : public std::runtime_error {
 public:
  enum class Code {
    kFileNotFound,
    kPermissionDenied,
    kIoError,
    kPathInvalid,
    kDirectoryNotEmpty,
    kFileExists,
    kUnknown
  };

  explicit FileException(Code code, const QString& message);
  explicit FileException(Code code, const QString& message,
                         const QString& path);

  Code code() const;
  const QString& path() const;
  const QString& message() const;

  QString toString() const;

 private:
  Code code_;
  QString path_;
  QString message_;
};

#endif  // FILE_CORE_COMMON_FILE_EXCEPTION_H_