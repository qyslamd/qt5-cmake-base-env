#include "user_manage_dialog.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "add_edit_user_dialog.h"
#include "auth/auth_service.h"
#include "auth/user_manager.h"

namespace {

QString RoleText(UserRole role) {
  return role == UserRole::kAdmin ? QStringLiteral("管理员")
                                  : QStringLiteral("普通用户");
}

}  // namespace

UserManageDialog::UserManageDialog(QWidget* parent) : QDialog(parent) {
  setObjectName(QStringLiteral("userManageDialog"));
  // 权限门控：无 user.manage 权限拒绝打开（入口应先行判断，此处双保险）
  if (!AuthService::instance().hasPermission(kPermUserManage)) {
    QMessageBox::warning(this, QStringLiteral("提示"),
                         QStringLiteral("无权限管理用户"));
    QTimer::singleShot(0, this, &QDialog::reject);
  }
  initUi();
  initSignals();
  refreshUsers();
}

void UserManageDialog::initUi() {
  setWindowTitle(QStringLiteral("用户管理"));
  resize(480, 320);

  auto* layout = new QVBoxLayout(this);

  table_ = new QTableWidget(0, 3, this);
  table_->setObjectName(QStringLiteral("userManageTable"));
  table_->setHorizontalHeaderLabels({QStringLiteral("用户名"),
                                     QStringLiteral("角色"),
                                     QStringLiteral("首次登录需改密")});
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->verticalHeader()->setVisible(false);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  layout->addWidget(table_, 1);

  auto* btn_row = new QHBoxLayout;
  btn_row->addStretch();
  add_button_ = new QPushButton(QStringLiteral("添加"), this);
  edit_button_ = new QPushButton(QStringLiteral("编辑"), this);
  delete_button_ = new QPushButton(QStringLiteral("删除"), this);
  auto* close_button = new QPushButton(QStringLiteral("关闭"), this);
  btn_row->addWidget(add_button_);
  btn_row->addWidget(edit_button_);
  btn_row->addWidget(delete_button_);
  btn_row->addWidget(close_button);
  layout->addLayout(btn_row);

  connect(close_button, &QPushButton::clicked, this, &QDialog::accept);
}

void UserManageDialog::initSignals() {
  connect(add_button_, &QPushButton::clicked, this,
          &UserManageDialog::onAddClicked);
  connect(edit_button_, &QPushButton::clicked, this,
          &UserManageDialog::onEditClicked);
  connect(delete_button_, &QPushButton::clicked, this,
          &UserManageDialog::onDeleteClicked);
}

void UserManageDialog::refreshUsers() {
  table_->setRowCount(0);
  const QList<User> users = UserManager::instance().allUsers();
  for (const User& user : users) {
    int row = table_->rowCount();
    table_->insertRow(row);
    table_->setItem(row, 0, new QTableWidgetItem(user.user_name_));
    table_->setItem(row, 1, new QTableWidgetItem(RoleText(user.role_)));
    table_->setItem(row, 2,
                    new QTableWidgetItem(user.must_change_password_
                                             ? QStringLiteral("是")
                                             : QStringLiteral("否")));
  }
}

void UserManageDialog::onAddClicked() {
  AddEditUserDialog dialog(AddEditUserDialog::Mode::kAdd, this);
  if (dialog.exec() == QDialog::Accepted) {
    refreshUsers();
  }
}

void UserManageDialog::onEditClicked() {
  int row = table_->currentRow();
  if (row < 0) {
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("请先选择要编辑的用户"));
    return;
  }
  const QString username = table_->item(row, 0)->text();
  const QList<User> users = UserManager::instance().allUsers();
  for (const User& user : users) {
    if (user.user_name_ == username) {
      AddEditUserDialog dialog(AddEditUserDialog::Mode::kModify, this);
      dialog.SetUserInfo(user.user_name_, user.role_);
      if (dialog.exec() == QDialog::Accepted) {
        refreshUsers();
      }
      return;
    }
  }
}

void UserManageDialog::onDeleteClicked() {
  int row = table_->currentRow();
  if (row < 0) {
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("请先选择要删除的用户"));
    return;
  }
  const QString username = table_->item(row, 0)->text();
  // admin 为内置管理员（与 UserManager 内置账号一致），不可删除
  if (username == QStringLiteral("admin")) {
    QMessageBox::warning(this, QStringLiteral("提示"),
                         QStringLiteral("内置管理员不可删除"));
    return;
  }
  // 不能删除当前登录用户（避免自锁）
  if (username == AuthService::instance().currentUser().user_name_) {
    QMessageBox::warning(this, QStringLiteral("提示"),
                         QStringLiteral("不能删除当前登录用户"));
    return;
  }
  if (QMessageBox::question(
          this, QStringLiteral("确认删除"),
          QStringLiteral("确定删除用户 %1 吗？").arg(username)) !=
      QMessageBox::Yes) {
    return;
  }
  if (!UserManager::instance().deleteUser(username)) {
    QMessageBox::warning(this, QStringLiteral("提示"),
                         UserManager::instance().lastError());
    return;
  }
  refreshUsers();
}
