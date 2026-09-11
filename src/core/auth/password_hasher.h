#ifndef FILE_CORE_AUTH_PASSWORD_HASHER_H_
#define FILE_CORE_AUTH_PASSWORD_HASHER_H_

#include <memory>

#include <QString>

// 密码哈希抽象。当前仅 SHA-256，后续如需加盐/换算法在此扩展。
class PasswordHasher {
 public:
  virtual ~PasswordHasher() = default;
  virtual QString hash(const QString& password) const = 0;
  virtual bool verify(const QString& password,
                      const QString& stored_hash) const = 0;
};

// SHA-256 哈希实现
class Sha256Hasher : public PasswordHasher {
 public:
  QString hash(const QString& password) const override;
  bool verify(const QString& password,
              const QString& stored_hash) const override;
};

// 哈希器工厂
class HasherFactory {
 public:
  static std::unique_ptr<PasswordHasher> create();
};

#endif  // FILE_CORE_AUTH_PASSWORD_HASHER_H_
