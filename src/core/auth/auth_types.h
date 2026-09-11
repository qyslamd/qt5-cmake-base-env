#ifndef FILE_CORE_AUTH_AUTH_TYPES_H_
#define FILE_CORE_AUTH_AUTH_TYPES_H_

#include <QString>

// 用户角色
enum class UserRole { kAdmin = 0, kUser };

// 用户信息
struct User {
  QString user_name_;
  QString password_;
  UserRole role_;
  bool must_change_password_;  // 首次登录是否强制改密

  User()
      : user_name_(),
        password_(),
        role_(UserRole::kUser),
        must_change_password_(false) {}
  User(const QString& user_name, const QString& password, UserRole role)
      : user_name_(user_name),
        password_(password),
        role_(role),
        must_change_password_(false) {}
};

inline constexpr const char* kPermUserManage = "user.manage";

#endif  // FILE_CORE_AUTH_AUTH_TYPES_H_
