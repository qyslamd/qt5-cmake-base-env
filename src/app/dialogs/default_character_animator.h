#ifndef FILE_APP_DIALOGS_DEFAULT_CHARACTER_ANIMATOR_H_
#define FILE_APP_DIALOGS_DEFAULT_CHARACTER_ANIMATOR_H_

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include "character_animator.h"

// 默认策略：原 CharacterPanel 的完整行为。
// 四个彩色角色 + 眨眼 + 偷看 + 错误抖动 + 鼠标跟随，
// 由面板的 16ms 逐帧定时器驱动插值（间隔由 animationIntervalMs() 报给面板）。
//
// 与面板的分工：
//   - 面板把窗口交互塞进 Input，本策略在 setInput() 里读；
//   - 本策略自己排眨眼 / 偷看 / 错误恢复的 singleShot 链，
//     用世代号保证同一条链同一时刻只有一份在跑；
//   - reset() 把世代号 +1、停 look_timer_，旧回调自然作废；
//     面板隐藏/销毁时的停帧由面板的定时器启停逻辑管，策略不管可见性。
class DefaultCharacterAnimator : public CharacterAnimator {
  Q_OBJECT

 public:
  explicit DefaultCharacterAnimator(QObject* parent = nullptr);

  void setInput(const Input& in) override;
  void onValidationFailed() override;

  bool isAnimated() const override { return true; }
  int animationIntervalMs() const override { return 16; }
  void tick() override;
  void paint(QPainter& p, const QRectF& rect) const override;
  void reset() override;

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
  void updateScene(const QSize& panel_size);
  void updateTargets();
  void scheduleBlink(int idx);
  void schedulePeek();

  void drawBackground(QPainter& p, const QRectF& rect) const;
  void drawLogo(QPainter& p, const QRectF& rect) const;
  void drawFooter(QPainter& p, const QRectF& rect) const;
  void drawChar(QPainter& p, int idx) const;

  QVector<Char> chars_;
  QTimer look_timer_;  // 打字后维持"看向别处"的短定时器
  int error_gen_ = 0;  // 错误恢复链的世代号
  int peek_gen_ = 0;   // 偷看链的世代号

  // 从 Input 展开的内部状态（避免每次都要经 input_ 访问）
  QPointF mouse_{-10000, -10000};
  bool typing_ = false;
  bool pwd_focus_ = false;
  bool pwd_visible_ = false;
  bool has_pwd_ = false;
  bool peeking_ = false;
  bool error_ = false;
  bool looking_ = false;
  qreal error_shake_ = 0;

  QRectF scene_;
  QString logo_text_ = QStringLiteral("CareerCompass");
  QStringList footer_links_{QStringLiteral("帮助"), QStringLiteral("关于")};
};

#endif  // FILE_APP_DIALOGS_DEFAULT_CHARACTER_ANIMATOR_H_