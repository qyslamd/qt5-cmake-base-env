#ifndef FILE_APP_DIALOGS_CHANGE_PASSWORD_DIALOG_H_
#define FILE_APP_DIALOGS_CHANGE_PASSWORD_DIALOG_H_

#include <QDialog>

class QLabel;
class QLineEdit;
class QToolButton;

// 修改密码对话框（强制改密/主动改密共用）
class ChangePasswordDialog : public QDialog {
  Q_OBJECT
 public:
  explicit ChangePasswordDialog(QWidget* parent = nullptr);

 private:
  void initUi();
  void initSignals();
  void onOkClicked();

  QLineEdit* old_password_edit_ = nullptr;
  QLineEdit* new_password_edit_ = nullptr;
  QLineEdit* confirm_password_edit_ = nullptr;
  QToolButton* ok_button_ = nullptr;
  QLabel* hint_label_ = nullptr;
};

#endif  // FILE_APP_DIALOGS_CHANGE_PASSWORD_DIALOG_H_
