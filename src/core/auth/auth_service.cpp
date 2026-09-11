#include "auth_service.h"

#include "logger/logger.h"
#include "user_manager.h"

AuthService& AuthService::instance() {
  static AuthService inst;
  return inst;
}

AuthService::AuthService() : QObject(nullptr) {
  UserManager::instance().loadUsers();
}

bool AuthService::login(const QString& username, const QString& password) {
  if (!UserManager::instance().loadUsers()) {
    emit loginFailed(UserManager::instance().lastError());
    return false;
  }
  User user = UserManager::instance().authenticate(username, password);
  if (user.user_name_.isEmpty()) {
    LOG_WARN("AUTH", "登录失败: 用户 {} 凭证错误", username.toStdString());
    emit loginFailed(UserManager::instance().lastError());
    return false;
  }
  logged_in_ = true;
  current_user_ = user;
  current_user_.password_.clear();  // 信号中不携带密码
  LOG_INFO("AUTH", "登录成功: 用户 {}, 角色 {}",
           current_user_.user_name_.toStdString(),
           static_cast<int>(current_user_.role_));
  emit loginSucceeded(current_user_);
  return true;
}

void AuthService::logout() {
  logged_in_ = false;
  current_user_ = User();
  emit loggedOut();
}

bool AuthService::isLoggedIn() const { return logged_in_; }

User AuthService::currentUser() const { return current_user_; }

UserRole AuthService::currentRole() const { return current_user_.role_; }

bool AuthService::hasPermission(const QString& permission) const {
  if (!logged_in_) {
    return false;
  }
  return permissionsForRole(current_user_.role_).contains(permission);
}

QSet<QString> AuthService::permissionsForRole(UserRole role) const {
  QSet<QString> perms;
  if (role == UserRole::kAdmin) {
    perms.insert(QString::fromLatin1(kPermUserManage));
  }
  return perms;
}
