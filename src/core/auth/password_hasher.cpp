#include "password_hasher.h"

#include <QCryptographicHash>

QString Sha256Hasher::hash(const QString& password) const {
  QByteArray digest =
      QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
  return QString::fromLatin1(digest.toHex());
}

bool Sha256Hasher::verify(const QString& password,
                          const QString& stored_hash) const {
  return hash(password) == stored_hash;
}

std::unique_ptr<PasswordHasher> HasherFactory::create() {
  return std::make_unique<Sha256Hasher>();
}
