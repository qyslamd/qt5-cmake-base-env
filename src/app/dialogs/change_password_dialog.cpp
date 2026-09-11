#include "change_password_dialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QVBoxLayout>

#include "auth/auth_service.h"
#include "auth/user_manager.h"

ChangePasswordDialog::ChangePasswordDialog(QWidget* parent) : QDialog(parent) {
  setObjectName(QStringLiteral("changePasswordDialog"));
  setWindowTitle(QStringLiteral("修改密码"));
  setFixedSize(360, 300);
  initUi();
  initSignals();
}

void ChangePasswordDialog::initUi() {
  auto* v_layout = new QVBoxLayout(this);
  v_layout->setContentsMargins(24, 24, 24, 24);
  v_layout->setSpacing(12);

  auto* title = new QLabel(QStringLiteral("修改密码"), this);
  title->setObjectName(QStringLiteral("changePasswordTitle"));

  old_password_edit_ = new QLineEdit(this);
  old_password_edit_->setObjectName(QStringLiteral("changePasswordOld"));
  old_password_edit_->setPlaceholderText(QStringLiteral("原密码"));
  old_password_edit_->setEchoMode(QLineEdit::Password);

  new_password_edit_ = new QLineEdit(this);
  new_password_edit_->setObjectName(QStringLiteral("changePasswordNew"));
  new_password_edit_->setPlaceholderText(QStringLiteral("新密码"));
  new_password_edit_->setEchoMode(QLineEdit::Password);

  confirm_password_edit_ = new QLineEdit(this);
  confirm_password_edit_->setObjectName(
      QStringLiteral("changePasswordConfirm"));
  confirm_password_edit_->setPlaceholderText(QStringLiteral("确认新密码"));
  confirm_password_edit_->setEchoMode(QLineEdit::Password);

  hint_label_ = new QLabel(this);
  hint_label_->setObjectName(QStringLiteral("changePasswordHint"));
  hint_label_->hide();

  ok_button_ = new QToolButton(this);
  ok_button_->setObjectName(QStringLiteral("changePasswordOk"));
  ok_button_->setText(QStringLiteral("确定"));

  auto* btn_row = new QHBoxLayout;
  btn_row->addStretch();
  btn_row->addWidget(ok_button_);

  v_layout->addWidget(title);
  v_layout->addWidget(old_password_edit_);
  v_layout->addWidget(new_password_edit_);
  v_layout->addWidget(confirm_password_edit_);
  v_layout->addWidget(hint_label_);
  v_layout->addStretch();
  v_layout->addLayout(btn_row);
}

void ChangePasswordDialog::initSignals() {
  connect(ok_button_, &QToolButton::clicked, this,
          &ChangePasswordDialog::onOkClicked);
}

void ChangePasswordDialog::onOkClicked() {
  hint_label_->hide();
  const QString& username = AuthService::instance().currentUser().user_name_;
  const QString& old_pass = old_password_edit_->text();
  const QString& new_pass = new_password_edit_->text();
  const QString& confirm = confirm_password_edit_->text();

  if (new_pass.isEmpty() || confirm.isEmpty()) {
    hint_label_->setText(QStringLiteral("新密码不能为空"));
    hint_label_->show();
    return;
  }
  if (new_pass != confirm) {
    hint_label_->setText(QStringLiteral("两次输入的新密码不一致"));
    hint_label_->show();
    return;
  }
  User checked = UserManager::instance().authenticate(username, old_pass);
  if (checked.user_name_.isEmpty()) {
    hint_label_->setText(QStringLiteral("原密码错误"));
    hint_label_->show();
    return;
  }
  if (!UserManager::instance().updateUser(
          username, new_pass, AuthService::instance().currentRole())) {
    hint_label_->setText(UserManager::instance().lastError());
    hint_label_->show();
    return;
  }
  accept();
}
