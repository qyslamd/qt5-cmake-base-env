#ifndef FILE_APP_DIALOGS_USER_MANAGE_DIALOG_H_
#define FILE_APP_DIALOGS_USER_MANAGE_DIALOG_H_

#include <QDialog>

class QPushButton;
class QTableWidget;

// 用户管理对话框：列出全部用户（用户名/角色/强制改密），支持添加/编辑/删除。
// 复用 UserManager；admin 用户不可删除。
class UserManageDialog : public QDialog {
  Q_OBJECT
 public:
  explicit UserManageDialog(QWidget* parent = nullptr);

 private:
  void initUi();
  void initSignals();
  void refreshUsers();
  void onAddClicked();
  void onEditClicked();
  void onDeleteClicked();

  QTableWidget* table_ = nullptr;
  QPushButton* add_button_ = nullptr;
  QPushButton* edit_button_ = nullptr;
  QPushButton* delete_button_ = nullptr;
};

#endif  // FILE_APP_DIALOGS_USER_MANAGE_DIALOG_H_
