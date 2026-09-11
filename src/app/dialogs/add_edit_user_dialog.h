#ifndef FILE_APP_DIALOGS_ADD_EDIT_USER_DIALOG_H_
#define FILE_APP_DIALOGS_ADD_EDIT_USER_DIALOG_H_

#include <QDialog>

#include "auth/auth_types.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

// 添加/编辑用户对话框（参考 eph213 AddEditUserDialog，无 .ui 代码布局）。
// 添加模式用户名可编辑；编辑模式用户名只读（以用户名定位更新）。
class AddEditUserDialog : public QDialog {
  Q_OBJECT
 public:
  enum class Mode { kAdd, kModify };

  AddEditUserDialog(Mode mode, QWidget* parent = nullptr);

  // 编辑模式预填用户信息
  void SetUserInfo(const QString& username, UserRole role);

  QString UserName() const;
  QString Password() const;
  UserRole Role() const;

 private:
  void initUi();
  void initSignals();
  void onSaveClicked();

  Mode mode_;
  QLineEdit* username_edit_ = nullptr;
  QLineEdit* password_edit_ = nullptr;
  QLineEdit* confirm_edit_ = nullptr;
  QComboBox* role_combo_ = nullptr;
  QLabel* hint_label_ = nullptr;
  QPushButton* save_button_ = nullptr;
  QPushButton* cancel_button_ = nullptr;
  QString editing_user_;  // 编辑模式用户名（不可改）
};

#endif  // FILE_APP_DIALOGS_ADD_EDIT_USER_DIALOG_H_
