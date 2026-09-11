#include "login_dialog.h"

#include <QCheckBox>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>

#include "auth/auth_service.h"
#include "change_password_dialog.h"
#include "config/config_defs.h"
#include "config/config_manager.h"

LoginDialog::LoginDialog(QWidget* parent) : QDialog(parent) {
  setObjectName(QStringLiteral("loginDialog"));
  setWindowTitle(QStringLiteral("登录"));
  // 无边框 + 透明背景，卡片阴影显示在透明区域
  setWindowFlag(Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setFixedSize(460, 370);
  initUi();
  initSignals();

  // 加载记住的凭据（ConfigManager -> FILE.ini）
  auto& cfg = ConfigManager::instance();
  QString saved_user =
      cfg.get<QString>(QString::fromLatin1(kConfigLoginRememberUsername));
  QString saved_pass =
      cfg.get<QString>(QString::fromLatin1(kConfigLoginRememberPassword));
  if (!saved_user.isEmpty()) {
    username_edit_->setText(saved_user);
    remember_check_box_->setChecked(true);
  }
  if (!saved_pass.isEmpty()) {
    password_edit_->setText(saved_pass);
  }
}

void LoginDialog::initUi() {
  // 顶层卡片（圆角白底 + 阴影）
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(10, 10, 10, 10);

  auto* content = new QFrame(this);
  content->setObjectName(QStringLiteral("loginContent"));
  auto* shadow = new QGraphicsDropShadowEffect(content);
  shadow->setBlurRadius(24);
  shadow->setOffset(0, 4);
  shadow->setColor(QColor(0, 0, 0, 60));
  content->setGraphicsEffect(shadow);

  auto* h_layout = new QHBoxLayout(content);
  h_layout->setContentsMargins(0, 0, 0, 0);
  h_layout->setSpacing(0);

  // 左侧品牌区
  auto* brand = new QWidget(content);
  brand->setObjectName(QStringLiteral("loginBrand"));
  brand->setFixedWidth(160);
  auto* brand_layout = new QVBoxLayout(brand);
  brand_layout->setContentsMargins(24, 24, 24, 24);
  brand_layout->setSpacing(4);
  auto* brand_title = new QLabel(QStringLiteral("EP-H276"), brand);
  brand_title->setObjectName(QStringLiteral("loginBrandTitle"));
  auto* brand_sub = new QLabel(QStringLiteral("设备控制软件"), brand);
  brand_sub->setObjectName(QStringLiteral("loginBrandSub"));
  auto* brand_hint = new QLabel(QStringLiteral("请登录以使用全部功能"), brand);
  brand_hint->setObjectName(QStringLiteral("loginBrandHint"));
  brand_layout->addWidget(brand_title);
  brand_layout->addWidget(brand_sub);
  brand_layout->addStretch();
  brand_layout->addWidget(brand_hint);
  h_layout->addWidget(brand);

  // 右侧表单区
  auto* form = new QWidget(content);
  form->setObjectName(QStringLiteral("loginForm"));
  auto* form_layout = new QVBoxLayout(form);
  form_layout->setContentsMargins(24, 24, 24, 24);
  form_layout->setSpacing(12);

  auto* title_row = new QHBoxLayout;
  auto* title_label = new QLabel(QStringLiteral("登录"), form);
  title_label->setObjectName(QStringLiteral("loginFormTitle"));
  auto* close_btn = new QToolButton(form);
  close_btn->setObjectName(QStringLiteral("loginCloseBtn"));
  close_btn->setFixedSize(28, 28);
  close_btn->setCursor(Qt::PointingHandCursor);
  close_btn->setText(QStringLiteral("×"));
  title_row->addWidget(title_label);
  title_row->addStretch();
  title_row->addWidget(close_btn);
  connect(close_btn, &QToolButton::clicked, this, &LoginDialog::reject);

  username_edit_ = new QLineEdit(form);
  username_edit_->setObjectName(QStringLiteral("loginUsername"));
  username_edit_->setPlaceholderText(QStringLiteral("admin"));

  password_edit_ = new QLineEdit(form);
  password_edit_->setObjectName(QStringLiteral("loginPassword"));
  password_edit_->setPlaceholderText(QStringLiteral("请输入密码"));
  password_edit_->setEchoMode(QLineEdit::Password);

  remember_check_box_ = new QCheckBox(QStringLiteral("记住密码"), form);
  remember_check_box_->setObjectName(QStringLiteral("loginRemember"));

  login_button_ = new QToolButton(form);
  login_button_->setObjectName(QStringLiteral("loginButton"));
  login_button_->setText(QStringLiteral("登录"));

  hint_label_ = new QLabel(form);
  hint_label_->setObjectName(QStringLiteral("loginHint"));
  hint_label_->hide();

  form_layout->addLayout(title_row);
  form_layout->addWidget(username_edit_);
  form_layout->addWidget(password_edit_);
  form_layout->addWidget(remember_check_box_);
  form_layout->addWidget(login_button_);
  form_layout->addWidget(hint_label_);
  form_layout->addStretch();
  h_layout->addWidget(form);

  outer->addWidget(content);
}

void LoginDialog::initSignals() {
  connect(login_button_, &QToolButton::clicked, this,
          &LoginDialog::onLoginClicked);
  connect(
      &AuthService::instance(), &AuthService::loginSucceeded, this, [this]() {
        auto& cfg = ConfigManager::instance();
        if (AuthService::instance().currentUser().must_change_password_) {
          // 首次登录强制改密：改密成功前不进入主功能
          ChangePasswordDialog dlg(this);
          if (dlg.exec() != QDialog::Accepted) {
            AuthService::instance().logout();
            password_edit_->clear();
            return;
          }
          // 改密后旧密码失效，清除记住的凭据避免下次预填错误密码
          cfg.set<QString>(QString::fromLatin1(kConfigLoginRememberUsername),
                           QString());
          cfg.set<QString>(QString::fromLatin1(kConfigLoginRememberPassword),
                           QString());
          remember_check_box_->setChecked(false);
        } else {
          if (remember_check_box_->isChecked()) {
            cfg.set<QString>(QString::fromLatin1(kConfigLoginRememberUsername),
                             username_edit_->text());
            cfg.set<QString>(QString::fromLatin1(kConfigLoginRememberPassword),
                             password_edit_->text());
          } else {
            cfg.set<QString>(QString::fromLatin1(kConfigLoginRememberUsername),
                             QString());
            cfg.set<QString>(QString::fromLatin1(kConfigLoginRememberPassword),
                             QString());
          }
        }
        cfg.sync();  // 立即落盘，避免异常退出丢失
        accept();
      });
  connect(&AuthService::instance(), &AuthService::loginFailed, this,
          [this](const QString& reason) {
            hint_label_->setText(reason);
            hint_label_->show();
            password_edit_->clear();
            password_edit_->setFocus();
          });
}

void LoginDialog::keyReleaseEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    onLoginClicked();
  }
  QDialog::keyReleaseEvent(event);
}

void LoginDialog::showEvent(QShowEvent* event) {
  QDialog::showEvent(event);
  // 屏幕居中（主窗口未显示）
  if (QScreen* screen = QGuiApplication::primaryScreen()) {
    move(screen->geometry().center() - rect().center());
  }
}

void LoginDialog::mousePressEvent(QMouseEvent* event) {
  // 仅从非交互控件区域开始拖拽（输入框/按钮/复选框除外）
  if (event->button() == Qt::LeftButton) {
    QWidget* child = childAt(event->pos());
    if (!child || child == this ||
        (qobject_cast<QLineEdit*>(child) == nullptr &&
         qobject_cast<QToolButton*>(child) == nullptr &&
         qobject_cast<QCheckBox*>(child) == nullptr)) {
      drag_offset_ = event->globalPos() - frameGeometry().topLeft();
      dragging_ = true;
    }
  }
  QDialog::mousePressEvent(event);
}

void LoginDialog::mouseMoveEvent(QMouseEvent* event) {
  if (dragging_ && (event->buttons() & Qt::LeftButton)) {
    move(event->globalPos() - drag_offset_);
  }
  QDialog::mouseMoveEvent(event);
}

void LoginDialog::mouseReleaseEvent(QMouseEvent* event) {
  dragging_ = false;
  QDialog::mouseReleaseEvent(event);
}

void LoginDialog::onLoginClicked() {
  hint_label_->hide();
  if (username_edit_->text().trimmed().isEmpty() ||
      password_edit_->text().isEmpty()) {
    hint_label_->setText(QStringLiteral("请输入用户名和密码"));
    hint_label_->show();
    return;
  }
  AuthService::instance().login(username_edit_->text().trimmed(),
                                password_edit_->text());
}
