#ifndef FILE_APP_DIALOGS_CHARACTER_PANEL_H_
#define FILE_APP_DIALOGS_CHARACTER_PANEL_H_

#include <QWidget>

#include "character_animator.h"

class QHideEvent;
class QShowEvent;
class QTimer;

// 左栏角色面板：只是个壳。
// - 持有一个 CharacterAnimator 策略，接管其所有权；
// - 持有一个动画定时器，只在控件可见且 animator 报告需要动画时运行；
// - 把窗口上的交互（输入、焦点、密码可见性、错误）以槽的形式收进来，
//   转成 animator 的 Input / 事件，不感知动画细节。
//
// 要"没有动画"就装一个 isAnimated() 恒为 false 的策略；要换/扩展行为
// 就实现一个新的 CharacterAnimator。对 LoginWindow 而言，下面的槽
// 接口不变。
class CharacterPanel : public QWidget {
  Q_OBJECT

 public:
  explicit CharacterPanel(QWidget* parent = nullptr);
  ~CharacterPanel() override;

  // 接管所有权。替换时旧的先 reset() 再销毁
  void setAnimator(CharacterAnimator* animator);
  CharacterAnimator* animator() const { return animator_; }

 public slots:
  void setTyping(bool on);
  void setPasswordFocused(bool on);
  void setPasswordVisible(bool on);
  void setHasPassword(bool on);
  void triggerError();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  // 把 input_ 推给 animator，并按需重估定时器状态
  void pushInput();
  // 依据可见性与 animator_->isAnimated() 启停定时器
  void updateTimerState();

  CharacterAnimator* animator_ = nullptr;
  CharacterAnimator::Input input_;
  QTimer* timer_ = nullptr;
};

#endif  // FILE_APP_DIALOGS_CHARACTER_PANEL_H_