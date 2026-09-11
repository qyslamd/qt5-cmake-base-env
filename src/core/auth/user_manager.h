#ifndef FILE_CORE_AUTH_USER_MANAGER_H_
#define FILE_CORE_AUTH_USER_MANAGER_H_

#include <QList>
#include <QString>

#include "auth_types.h"

// 用户数据管理：加载/保存用户到 AppData 下的 users.json
class UserManager {
 public:
  static UserManager& instance();

  bool loadUsers();
  bool saveUsers();
  QList<User> allUsers() const;
  User authenticate(const QString& username, const QString& password) const;
  bool addUser(const QString& username, const QString& password,
               UserRole role = UserRole::kUser);
  bool updateUser(const QString& username, const QString& new_password,
                  UserRole new_role);
  bool deleteUser(const QString& username);
  QString lastError() const;

 private:
  UserManager() = default;
  ~UserManager() = default;
  UserManager(const UserManager&) = delete;
  UserManager& operator=(const UserManager&) = delete;
  QString dataFilePath() const;
  int findUserIndex(const QString& username) const;

  QList<User> users_;
  mutable QString last_error_;
};

#endif  // FILE_CORE_AUTH_USER_MANAGER_H_
