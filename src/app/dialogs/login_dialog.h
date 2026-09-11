#ifndef FILE_APP_DIALOGS_LOGIN_DIALOG_H_
#define FILE_APP_DIALOGS_LOGIN_DIALOG_H_

#include <QDialog>
#include <QPoint>

class QCheckBox;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QMouseEvent;
class QToolButton;

// 登录对话框：无边框 + 透明背景，顶层 QFrame 卡片承载所有控件。
// 登录成功前主窗口不显示，屏幕居中显示。
class LoginDialog : public QDialog {
  Q_OBJECT
 public:
  explicit LoginDialog(QWidget* parent = nullptr);

 protected:
  void keyReleaseEvent(QKeyEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 private:
  void initUi();
  void initSignals();
  void onLoginClicked();

  QLineEdit* username_edit_ = nullptr;
  QLineEdit* password_edit_ = nullptr;
  QCheckBox* remember_check_box_ = nullptr;
  QToolButton* login_button_ = nullptr;
  QLabel* hint_label_ = nullptr;
  QPoint drag_offset_;
  bool dragging_ = false;
};

#endif  // FILE_APP_DIALOGS_LOGIN_DIALOG_H_
