#ifndef FILE_EXAMPLES_NEW_LOGIN_DIALOG_LOGIN_WINDOW_H_
#define FILE_EXAMPLES_NEW_LOGIN_DIALOG_LOGIN_WINDOW_H_

#include <QAbstractButton>
#include <QColor>
#include <QIcon>
#include <QPointF>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPropertyAnimation;
class QToolButton;

// ============ 自绘按钮：hover 时文字右滑 + 紫覆盖层 + 箭头 ============
class HoverButton : public QAbstractButton {
  Q_OBJECT
  Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)
 public:
  enum Variant { Primary, Secondary };

  HoverButton(const QString& text, Variant v, QWidget* parent = nullptr);

  void setLeadingIcon(const QIcon& icon);
  void setAccentColor(const QColor& c);

  void setText(const QString& t);

  qreal hoverProgress() const { return hp_; }
  void setHoverProgress(qreal p);

  QSize sizeHint() const override { return QSize(300, 50); }

 protected:
  void paintEvent(QPaintEvent*) override;
  void enterEvent(QEvent*) override;
  void leaveEvent(QEvent*) override;

 private:
  QString text_;
  QIcon icon_;
  Variant variant_;
  QColor accent_{"#5b21b6"};
  qreal hp_ = 0.0;
  QPropertyAnimation* anim_ = nullptr;
};

// ============ 左侧角色面板：QPainter 全自绘 ============
class CharacterPanel : public QWidget {
  Q_OBJECT
 public:
  explicit CharacterPanel(QWidget* parent = nullptr);

  void setTyping(bool on);
  void setPasswordFocused(bool on);
  void setPasswordVisible(bool on);
  void setHasPassword(bool on);
  void triggerError();

 protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  bool eventFilter(QObject*, QEvent*) override;

 private slots:
  void onTick();

 private:
  struct Char {
    QColor body;
    QRectF base;
    qreal corner = 10;
    bool half_circle = false;

    QPointF eye_anchor;
    qreal eye_size = 18;
    qreal eye_gap = 28;
    bool has_eyeball = true;
    qreal pupil_ratio = 0.4;
    bool has_mouth = false;
    QPointF mouth_anchor;
    qreal mouth_w = 50;
    qreal mouth_h = 4;

    // 目标值
    qreal skew_t = 0;
    qreal height_t = 0;
    QPointF offset_t;
    QPointF pupil_t;
    QPointF eye_move_t;
    bool sad_t = false;

    // 当前值（每帧插值）
    qreal skew = 0;
    qreal height = 0;
    QPointF offset;
    QPointF pupil;
    QPointF eye_move;
    qreal eye_h = 1.0;
    bool blinking = false;
    bool sad = false;
  };

  void initChars();
  void updateScene();
  void updateTargets();
  void scheduleBlink(int idx);
  void schedulePeek();

  void drawBackground(QPainter& p);
  void drawLogo(QPainter& p);
  void drawFooter(QPainter& p);
  void drawChar(QPainter& p, int idx);

  QVector<Char> chars_;
  QTimer tick_;
  QTimer look_timer_;
  int error_gen_ = 0;
  // 偷看链的世代号：重复触发时旧的链作废，保证同时只有一条在跑
  int peek_gen_ = 0;

  QPointF mouse_{-10000, -10000};
  bool typing_ = false;
  bool pwd_focus_ = false;
  bool pwd_visible_ = false;
  bool has_pwd_ = false;
  // 显示密码时的"偷看"：只有第一个角色会，且是周期性的（原版 isPurplePeeking）
  bool peeking_ = false;
  bool error_ = false;
  bool looking_ = false;
  qreal error_shake_ = 0;

  QRectF scene_;
  QString logo_text_ = QStringLiteral("CareerCompass");
  // 客户端程序：footer 是本地的"帮助 / 关于"，不是 Web 的隐私/服务条款
  QStringList footer_links_{QStringLiteral("帮助"), QStringLiteral("关于")};
};

// ============ 主窗口：左角色 + 右表单 ============
class LoginWindow : public QWidget {
  Q_OBJECT
 public:
  explicit LoginWindow(QWidget* parent = nullptr);

 signals:
  void loginSucceeded();
  void loginCanceled();

 private:
  void initUi();
  void initSignals();
  void onLoginClicked();

  CharacterPanel* left_ = nullptr;
  QWidget* right_ = nullptr;

  QLineEdit* email_ = nullptr;
  QLineEdit* password_ = nullptr;
  QToolButton* eye_btn_ = nullptr;
  QCheckBox* remember_ = nullptr;
  HoverButton* login_btn_ = nullptr;

  QLabel* email_label_ = nullptr;
  QLabel* pwd_label_ = nullptr;
  QLabel* err_label_ = nullptr;

  bool show_pwd_ = false;
};

#endif  // FILE_EXAMPLES_NEW_LOGIN_DIALOG_LOGIN_WINDOW_H_