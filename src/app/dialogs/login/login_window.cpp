#include "login_window.h"

#include <functional>

#include <QCheckBox>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include "auth/auth_service.h"
#include "character_panel.h"
#include "config/config_defs.h"
#include "config/config_manager.h"
#include "dialogs/change_password_dialog.h"
#include "dialogs/login/login_animator_factory.h"
#include "hover_button.h"         // 来自 ${APP_TARGET_PREFIX}_controls
#include "password_eye_button.h"  // 同上

namespace {

// 版式（字号/字重/字距）在代码里表达，颜色/边框归样式表——
// 两条界面路线共用
void ApplyFont(QWidget* w, int pixel_size, int weight = QFont::Normal,
               qreal letter_spacing = 0.0) {
  QFont f = w->font();
  f.setPixelSize(pixel_size);
  f.setWeight(weight);
  if (letter_spacing != 0.0) {
    f.setLetterSpacing(QFont::AbsoluteSpacing, letter_spacing);
  }
  w->setFont(f);
}

}  // namespace

LoginWindow::LoginWindow(QWidget* parent) : QDialog(parent) {
  setObjectName(QStringLiteral("loginDialog"));
  setWindowTitle(QStringLiteral("登录"));
  setMinimumSize(900, 580);
  // 初始就按最小尺寸开，别一上来占掉半个屏
  resize(minimumSize());
  initUi();
  initSignals();

  // 加载记住的凭据（ConfigManager -> FILE.ini）
  auto& cfg = ConfigManager::instance();
  const QString saved_user =
      cfg.get<QString>(QString::fromLatin1(kConfigLoginRememberUsername));
  const QString saved_pass =
      cfg.get<QString>(QString::fromLatin1(kConfigLoginRememberPassword));
  if (!saved_user.isEmpty()) {
    username_->setText(saved_user);
    remember_->setChecked(true);
  }
  if (!saved_pass.isEmpty()) {
    password_->setText(saved_pass);
  }
}

void LoginWindow::initUi() {
  auto* root = new QHBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  // 左栏：面板自带默认策略，这里按 login/animation 换成配置指定的那支
  left_ = new CharacterPanel(this);
  left_->setAnimator(CreateLoginAnimator());
  left_->setObjectName(QStringLiteral("loginLeftPanel"));
  left_->setMinimumWidth(440);
  left_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  root->addWidget(left_, 1);

  right_ = new QWidget(this);
  right_->setObjectName(QStringLiteral("loginRightPanel"));
  right_->setMinimumWidth(440);
  right_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  root->addWidget(right_, 1);

  auto* outer = new QVBoxLayout(right_);
  outer->setContentsMargins(40, 40, 40, 40);
  outer->addStretch();

  auto* form_host = new QWidget(right_);
  form_host->setObjectName(QStringLiteral("loginFormHost"));
  form_host->setMaximumWidth(400);
  auto* form = new QVBoxLayout(form_host);
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(0);

  // Sparkle 图标
  {
    auto* row = new QHBoxLayout();
    row->addStretch();
    auto* spark = new QLabel(form_host);
    spark->setObjectName(QStringLiteral("loginSparkle"));
    spark->setFixedSize(32, 32);
    ApplyFont(spark, 24);
    spark->setText(QStringLiteral("✦"));
    spark->setAlignment(Qt::AlignCenter);
    row->addWidget(spark);
    row->addStretch();
    form->addLayout(row);
    form->addSpacing(24);
  }

  // 标题
  {
    auto* h1 = new QLabel(QStringLiteral("欢迎回来！"), form_host);
    h1->setObjectName(QStringLiteral("loginTitle"));
    h1->setAlignment(Qt::AlignCenter);
    ApplyFont(h1, 26, QFont::Bold, -0.5);
    form->addWidget(h1);
    form->addSpacing(4);

    auto* p = new QLabel(QStringLiteral("请输入您的账号信息"), form_host);
    p->setObjectName(QStringLiteral("loginSubtitle"));
    p->setAlignment(Qt::AlignCenter);
    ApplyFont(p, 13);
    form->addWidget(p);
    form->addSpacing(32);
  }

  auto make_label = [&](const QString& name, const QString& text) {
    auto* l = new QLabel(text, form_host);
    l->setObjectName(name);
    ApplyFont(l, 13, QFont::Medium);
    return l;
  };

  // 用户名
  username_label_ = make_label(QStringLiteral("loginUsernameLabel"),
                               QStringLiteral("用户名"));
  form->addWidget(username_label_);
  form->addSpacing(8);

  username_ = new QLineEdit(form_host);
  username_->setObjectName(QStringLiteral("loginUsername"));
  username_->setPlaceholderText(QStringLiteral("admin"));
  username_->setMinimumHeight(40);
  ApplyFont(username_, 14);
  form->addWidget(username_);
  form->addSpacing(20);

  // 密码
  pwd_label_ =
      make_label(QStringLiteral("loginPasswordLabel"), QStringLiteral("密码"));
  form->addWidget(pwd_label_);
  form->addSpacing(8);

  {
    auto* wrap = new QWidget(form_host);
    wrap->setObjectName(QStringLiteral("loginFieldWrap"));
    auto* hl = new QHBoxLayout(wrap);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    password_ = new QLineEdit(wrap);
    password_->setObjectName(QStringLiteral("loginPassword"));
    password_->setPlaceholderText(QStringLiteral("••••••••"));
    password_->setEchoMode(QLineEdit::Password);
    password_->setMinimumHeight(40);
    ApplyFont(password_, 14);
    hl->addWidget(password_, 1);

    eye_btn_ = new PasswordEyeButton(wrap);
    eye_btn_->setObjectName(QStringLiteral("loginTogglePwd"));
    hl->addWidget(eye_btn_, 0, Qt::AlignVCenter);

    form->addWidget(wrap);
  }
  form->addSpacing(20);

  // 记住密码
  {
    auto* row = new QHBoxLayout();
    remember_ = new QCheckBox(QStringLiteral("记住密码"), form_host);
    remember_->setObjectName(QStringLiteral("loginRemember"));
    remember_->setChecked(true);
    ApplyFont(remember_, 13);
    row->addWidget(remember_);
    row->addStretch();
    form->addLayout(row);
    form->addSpacing(24);
  }

  // 错误提示：红字来自底座的角色规则，横幅的壳在项目层
  err_label_ = new QLabel(form_host);
  err_label_->setObjectName(QStringLiteral("loginError"));
  err_label_->setProperty("role", "error");
  err_label_->setVisible(false);
  err_label_->setWordWrap(true);
  form->addWidget(err_label_);
  form->addSpacing(16);

  // 主按钮（库版 HoverButton：Primary 无前导图标）
  login_btn_ =
      new HoverButton(QStringLiteral("登录"), HoverButton::Primary, form_host);
  login_btn_->setObjectName(QStringLiteral("loginSubmit"));
  form->addWidget(login_btn_);

  outer->addWidget(form_host, 0, Qt::AlignHCenter);
  outer->addStretch();
}

void LoginWindow::initSignals() {
  // ---- 输入框 -> 事件出口 ----
  connect(username_, &QLineEdit::textEdited, this,
          [this]() { emit typingChanged(true); });
  connect(username_, &QLineEdit::editingFinished, this,
          [this]() { emit typingChanged(false); });

  connect(password_, &QLineEdit::textChanged, this, [this](const QString& s) {
    emit passwordPresenceChanged(!s.isEmpty());
  });
  connect(password_, &QLineEdit::textEdited, this,
          [this]() { emit typingChanged(true); });
  connect(password_, &QLineEdit::editingFinished, this,
          [this]() { emit typingChanged(false); });

  // 密码框焦点：用事件过滤器抓 FocusIn / FocusOut
  {
    class FocusWatcher : public QObject {
     public:
      std::function<void(bool)> cb;
      bool eventFilter(QObject*, QEvent* e) override {
        if (e->type() == QEvent::FocusIn && cb) {
          cb(true);
        }
        if (e->type() == QEvent::FocusOut && cb) {
          cb(false);
        }
        return false;
      }
    };
    auto* w = new FocusWatcher();
    w->setParent(password_);  // 随密码框一起销毁
    w->cb = [this](bool in) { emit passwordFocusChanged(in); };
    password_->installEventFilter(w);
  }

  // 眼睛切换：状态在按钮上（checked = 密码可见），窗口不再自己记
  connect(eye_btn_, &QAbstractButton::toggled, this, [this](bool visible) {
    password_->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
    emit passwordVisibilityChanged(visible);
  });

  connect(login_btn_, &QAbstractButton::clicked, this,
          &LoginWindow::onLoginClicked);

  // ---- 事件出口 -> CharacterPanel 的槽 ----
  // 本窗口不直接命令动画，只把交互事实转达出去
  connect(this, &LoginWindow::typingChanged, left_, &CharacterPanel::setTyping);
  connect(this, &LoginWindow::passwordFocusChanged, left_,
          &CharacterPanel::setPasswordFocused);
  connect(this, &LoginWindow::passwordVisibilityChanged, left_,
          &CharacterPanel::setPasswordVisible);
  connect(this, &LoginWindow::passwordPresenceChanged, left_,
          &CharacterPanel::setHasPassword);
  connect(this, &LoginWindow::validationFailed, left_,
          &CharacterPanel::triggerError);

  // ---- 鉴权结果 ----
  // 成功链与旧 LoginDialog 一致：首登强制改密 → 记住凭据 → accept()
  connect(
      &AuthService::instance(), &AuthService::loginSucceeded, this, [this]() {
        auto& cfg = ConfigManager::instance();
        if (AuthService::instance().currentUser().must_change_password_) {
          // 首次登录强制改密：改密成功前不进入主功能
          ChangePasswordDialog dlg(this);
          if (dlg.exec() != QDialog::Accepted) {
            AuthService::instance().logout();
            password_->clear();
            return;
          }
          // 改密后旧密码失效，清除记住的凭据避免下次预填错误密码
          cfg.set<QString>(QString::fromLatin1(kConfigLoginRememberUsername),
                           QString());
          cfg.set<QString>(QString::fromLatin1(kConfigLoginRememberPassword),
                           QString());
          remember_->setChecked(false);
        } else {
          if (remember_->isChecked()) {
            cfg.set<QString>(QString::fromLatin1(kConfigLoginRememberUsername),
                             username_->text());
            cfg.set<QString>(QString::fromLatin1(kConfigLoginRememberPassword),
                             password_->text());
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
            err_label_->setText(reason);
            err_label_->setVisible(true);
            password_->clear();
            password_->setFocus();
            emit validationFailed();
          });
}

void LoginWindow::onLoginClicked() {
  const QString user = username_->text().trimmed();
  const QString pw = password_->text();

  err_label_->setVisible(false);

  if (user.isEmpty() || pw.isEmpty()) {
    err_label_->setText(QStringLiteral("请输入用户名和密码。"));
    err_label_->setVisible(true);
    emit validationFailed();
    return;
  }

  // 同步鉴权（本地用户库）。成败都由 AuthService 的信号回调接手：
  // 成功走改密/记住凭据/accept()，失败走 loginFailed 分支
  AuthService::instance().login(user, pw);
}

void LoginWindow::keyReleaseEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    onLoginClicked();
  }
  QDialog::keyReleaseEvent(event);
}
