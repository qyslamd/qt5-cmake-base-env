#ifndef FILE_APP_DIALOGS_LOGIN_WINDOW_H_
#define FILE_APP_DIALOGS_LOGIN_WINDOW_H_

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QToolButton;

class CharacterPanel;
class HoverButton;

// 登录窗口：左侧角色动画面板 + 右侧表单。只做业务 + 组合，左栏动画不在此
// 感知。交互过程中产生的"动作/事件"通过信号出口，由 initSignals 接到
// CharacterPanel 的槽上。
//
// 鉴权走 AuthService（真账号体系）；loginSucceeded 后由 AuthService 的信号
// 驱动首登改密与记住凭据，最后 accept() 关窗。
class LoginWindow : public QDialog {
  Q_OBJECT

 public:
  explicit LoginWindow(QWidget* parent = nullptr);

  // 左栏面板的出口：换动画策略（如 StaticCharacterAnimator）用。
  // 窗口只转发，不解释 CharacterAnimator 是什么
  CharacterPanel* characterPanel() const { return left_; }

 signals:
  // ---- 交互事件出口 ----
  // 输入进行中（两个输入框共用一条：任一框正在编辑即为 true）
  void typingChanged(bool on);
  // 密码框获得 / 失去焦点
  void passwordFocusChanged(bool on);
  // 眼睛按钮切换后，密码当前是否可见
  void passwordVisibilityChanged(bool visible);
  // 密码框内容是否非空
  void passwordPresenceChanged(bool has);
  // 校验失败（邮箱格式、密码长度、鉴权失败三处共用）
  void validationFailed();

 protected:
  // 回车即登录。用 keyRelease：keyPress 会被 QLineEdit 先吃掉（旧登录同此）
  void keyReleaseEvent(QKeyEvent* event) override;

 private:
  void initUi();
  void initSignals();
  void onLoginClicked();

  CharacterPanel* left_ = nullptr;
  QWidget* right_ = nullptr;

  QLineEdit* username_ = nullptr;
  QLineEdit* password_ = nullptr;
  QToolButton* eye_btn_ = nullptr;
  QCheckBox* remember_ = nullptr;
  HoverButton* login_btn_ = nullptr;

  QLabel* username_label_ = nullptr;
  QLabel* pwd_label_ = nullptr;
  QLabel* err_label_ = nullptr;

  bool show_pwd_ = false;
};

#endif  // FILE_APP_DIALOGS_LOGIN_WINDOW_H_
