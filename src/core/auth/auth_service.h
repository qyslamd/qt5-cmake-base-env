#ifndef FILE_CORE_AUTH_AUTH_SERVICE_H_
#define FILE_CORE_AUTH_AUTH_SERVICE_H_

#include <QObject>
#include <QSet>

#include "auth_types.h"

// 认证服务：登录状态、当前用户、权限判定
class AuthService : public QObject {
  Q_OBJECT

 public:
  static AuthService& instance();

  bool login(const QString& username, const QString& password);
  void logout();
  bool isLoggedIn() const;
  User currentUser() const;
  UserRole currentRole() const;
  bool hasPermission(const QString& permission) const;

 signals:
  void loginSucceeded(const User& user);
  void loginFailed(const QString& reason);
  void loggedOut();

 private:
  AuthService();
  ~AuthService() override = default;
  AuthService(const AuthService&) = delete;
  AuthService& operator=(const AuthService&) = delete;
  QSet<QString> permissionsForRole(UserRole role) const;

  bool logged_in_ = false;
  User current_user_;
};

#endif  // FILE_CORE_AUTH_AUTH_SERVICE_H_
