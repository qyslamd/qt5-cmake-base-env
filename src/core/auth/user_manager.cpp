#include "user_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include "logger/logger.h"
#include "password_hasher.h"

namespace {

// 默认管理员账号，首次运行自动创建
constexpr const char* kDefaultAdmin = "admin";
constexpr const char* kDefaultPassword = "123";

}  // namespace

UserManager& UserManager::instance() {
  static UserManager inst;
  return inst;
}

QString UserManager::dataFilePath() const {
  return QDir(QStandardPaths::writableLocation(
      QStandardPaths::AppDataLocation))
      .absoluteFilePath("users.json");
}

bool UserManager::loadUsers() {
  users_.clear();
  QString path = dataFilePath();
  if (!QFile::exists(path)) {
    // 首次运行：创建默认管理员，需强制改密
    User admin{QString::fromLatin1(kDefaultAdmin),
               HasherFactory::create()->hash(
                   QString::fromLatin1(kDefaultPassword)),
               UserRole::kAdmin};
    admin.must_change_password_ = true;
    users_.append(admin);
    LOG_INFO("AUTH", "首次运行，创建默认管理员 admin");
    return saveUsers();
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    last_error_ = QStringLiteral("无法打开用户文件");
    LOG_ERROR("AUTH", "打开用户文件失败: {}", last_error_.toStdString());
    return false;
  }
  QJsonDocument doc =
      QJsonDocument::fromJson(QByteArray::fromBase64(file.readAll()));
  file.close();
  if (!doc.isArray()) {
    last_error_ = QStringLiteral("用户文件格式错误");
    LOG_ERROR("AUTH", "用户文件格式错误: {}", path.toStdString());
    return false;
  }
  for (const auto& val : doc.array()) {
    QJsonObject obj = val.toObject();
    User u{obj.value(QStringLiteral("UserName")).toString(),
           obj.value(QStringLiteral("Password")).toString(),
           static_cast<UserRole>(
               obj.value(QStringLiteral("Role")).toInt())};
    u.must_change_password_ =
        obj.value(QStringLiteral("MustChangePassword")).toBool();
    users_.append(u);
  }
  if (users_.isEmpty()) {
    // 兜底：文件存在但无任何账号，重建默认 admin，避免系统锁死
    User admin{QString::fromLatin1(kDefaultAdmin),
               HasherFactory::create()->hash(
                   QString::fromLatin1(kDefaultPassword)),
               UserRole::kAdmin};
    admin.must_change_password_ = true;
    users_.append(admin);
    LOG_WARN("AUTH", "用户文件为空，重建默认 admin");
    return saveUsers();
  }
  return true;
}

bool UserManager::saveUsers() {
  QDir().mkpath(QFileInfo(dataFilePath()).absolutePath());
  QFile file(dataFilePath());
  if (!file.open(QIODevice::WriteOnly)) {
    last_error_ = QStringLiteral("无法写入用户文件");
    LOG_ERROR("AUTH", "写入用户文件失败: {}", last_error_.toStdString());
    return false;
  }
  QJsonArray arr;
  for (const auto& u : users_) {
    QJsonObject obj;
    obj[QStringLiteral("UserName")] = u.user_name_;
    obj[QStringLiteral("Password")] = u.password_;
    obj[QStringLiteral("Role")] = static_cast<int>(u.role_);
    obj[QStringLiteral("MustChangePassword")] = u.must_change_password_;
    arr.append(obj);
  }
  file.write(QJsonDocument(arr).toJson().toBase64());
  file.close();
  return true;
}

QList<User> UserManager::allUsers() const { return users_; }

User UserManager::authenticate(const QString& username,
                               const QString& password) const {
  auto hasher = HasherFactory::create();
  for (const auto& u : users_) {
    if (u.user_name_ == username && hasher->verify(password, u.password_)) {
      return u;
    }
  }
  last_error_ = QStringLiteral("用户名或密码错误");
  return User();
}

bool UserManager::addUser(const QString& username, const QString& password,
                          UserRole role) {
  QString name = username.trimmed();
  if (name.isEmpty()) {
    last_error_ = QStringLiteral("用户名不能为空");
    return false;
  }
  if (findUserIndex(name) >= 0) {
    last_error_ = QStringLiteral("用户已存在");
    return false;
  }
  users_.append(User{name, HasherFactory::create()->hash(password), role});
  LOG_INFO("AUTH", "新增用户: {}", name.toStdString());
  return saveUsers();
}

bool UserManager::updateUser(const QString& username,
                             const QString& new_password, UserRole new_role) {
  int idx = findUserIndex(username);
  if (idx < 0) {
    last_error_ = QStringLiteral("用户不存在");
    return false;
  }
  User& u = users_[idx];
  if (!new_password.isEmpty()) {
    u.password_ = HasherFactory::create()->hash(new_password);
    u.must_change_password_ = false;  // 改密后解除强制
    LOG_INFO("AUTH", "用户 {} 已修改密码", username.toStdString());
  }
  u.role_ = new_role;
  return saveUsers();
}

bool UserManager::deleteUser(const QString& username) {
  if (username == QString::fromLatin1(kDefaultAdmin)) {
    last_error_ = QStringLiteral("admin 用户不可删除");
    return false;
  }
  int idx = findUserIndex(username);
  if (idx < 0) {
    last_error_ = QStringLiteral("用户不存在");
    return false;
  }
  users_.removeAt(idx);
  LOG_INFO("AUTH", "删除用户: {}", username.toStdString());
  return saveUsers();
}

QString UserManager::lastError() const { return last_error_; }

int UserManager::findUserIndex(const QString& username) const {
  for (int i = 0; i < users_.size(); ++i) {
    if (users_[i].user_name_ == username) {
      return i;
    }
  }
  return -1;
}
