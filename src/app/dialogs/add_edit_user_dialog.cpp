#include "add_edit_user_dialog.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "auth/user_manager.h"

AddEditUserDialog::AddEditUserDialog(Mode mode, QWidget* parent)
    : QDialog(parent), mode_(mode) {
  setObjectName(QStringLiteral("addEditUserDialog"));
  initUi();
  initSignals();
}

void AddEditUserDialog::SetUserInfo(const QString& username, UserRole role) {
  editing_user_ = username;
  username_edit_->setText(username);
  username_edit_->setReadOnly(true);
  role_combo_->setCurrentIndex(static_cast<int>(role));
}

QString AddEditUserDialog::UserName() const {
  return username_edit_->text().trimmed();
}

QString AddEditUserDialog::Password() const { return password_edit_->text(); }

UserRole AddEditUserDialog::Role() const {
  return static_cast<UserRole>(role_combo_->currentData().toInt());
}

void AddEditUserDialog::initUi() {
  setWindowTitle(mode_ == Mode::kAdd ? QStringLiteral("添加用户")
                                     : QStringLiteral("编辑用户"));
  setFixedSize(320, 240);

  auto* layout = new QVBoxLayout(this);

  auto* username_row = new QHBoxLayout;
  username_row->addWidget(new QLabel(QStringLiteral("用户名:"), this));
  username_edit_ = new QLineEdit(this);
  username_edit_->setObjectName(QStringLiteral("addEditUserUsername"));
  username_row->addWidget(username_edit_, 1);
  layout->addLayout(username_row);

  auto* password_row = new QHBoxLayout;
  password_row->addWidget(new QLabel(QStringLiteral("密码:"), this));
  password_edit_ = new QLineEdit(this);
  password_edit_->setEchoMode(QLineEdit::Password);
  password_edit_->setObjectName(QStringLiteral("addEditUserPassword"));
  password_row->addWidget(password_edit_, 1);
  layout->addLayout(password_row);

  auto* confirm_row = new QHBoxLayout;
  confirm_row->addWidget(new QLabel(QStringLiteral("确认密码:"), this));
  confirm_edit_ = new QLineEdit(this);
  confirm_edit_->setEchoMode(QLineEdit::Password);
  confirm_edit_->setObjectName(QStringLiteral("addEditUserConfirm"));
  confirm_row->addWidget(confirm_edit_, 1);
  layout->addLayout(confirm_row);

  auto* role_row = new QHBoxLayout;
  role_row->addWidget(new QLabel(QStringLiteral("角色:"), this));
  role_combo_ = new QComboBox(this);
  role_combo_->addItem(QStringLiteral("管理员"),
                       static_cast<int>(UserRole::kAdmin));
  role_combo_->addItem(QStringLiteral("普通用户"),
                       static_cast<int>(UserRole::kUser));
  // 默认普通用户（最小权限）
  role_combo_->setCurrentIndex(static_cast<int>(UserRole::kUser));
  role_row->addWidget(role_combo_);
  role_row->addStretch();
  layout->addLayout(role_row);

  hint_label_ = new QLabel(this);
  hint_label_->setObjectName(QStringLiteral("addEditUserHint"));
  hint_label_->hide();
  layout->addWidget(hint_label_);

  auto* btn_row = new QHBoxLayout;
  btn_row->addStretch();
  save_button_ = new QPushButton(
      mode_ == Mode::kAdd ? QStringLiteral("添加") : QStringLiteral("保存"),
      this);
  cancel_button_ = new QPushButton(QStringLiteral("取消"), this);
  btn_row->addWidget(save_button_);
  btn_row->addWidget(cancel_button_);
  layout->addLayout(btn_row);
}

void AddEditUserDialog::initSignals() {
  connect(save_button_, &QPushButton::clicked, this,
          &AddEditUserDialog::onSaveClicked);
  connect(cancel_button_, &QPushButton::clicked, this, &QDialog::reject);
}

void AddEditUserDialog::onSaveClicked() {
  hint_label_->hide();
  QString username = UserName();
  QString password = Password();
  QString confirm = confirm_edit_->text();
  if (username.isEmpty()) {
    hint_label_->setText(QStringLiteral("用户名不能为空"));
    hint_label_->show();
    return;
  }
  // 添加必须设密码；编辑允许空密码（保持原密码，见 UserManager::updateUser）
  if (mode_ == Mode::kAdd && password.isEmpty()) {
    hint_label_->setText(QStringLiteral("密码不能为空"));
    hint_label_->show();
    return;
  }
  if (!password.isEmpty() && password != confirm) {
    hint_label_->setText(QStringLiteral("两次输入的密码不一致"));
    hint_label_->show();
    return;
  }
  UserRole role = Role();
  bool ok =
      (mode_ == Mode::kAdd)
          ? UserManager::instance().addUser(username, password, role)
          : UserManager::instance().updateUser(editing_user_, password, role);
  if (!ok) {
    QMessageBox::warning(this, QStringLiteral("提示"),
                         UserManager::instance().lastError());
    return;
  }
  accept();
}
