#include "character_panel.h"

#include <QApplication>
#include <QHideEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>

#include "default_character_animator.h"

CharacterPanel::CharacterPanel(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setAttribute(Qt::WA_OpaquePaintEvent, false);

  // 跟随全局鼠标，供策略做视线跟随。策略本身不装任何事件过滤器
  qApp->installEventFilter(this);

  input_.panel_size = size();

  // 定时器要先于 animator 建好：setAnimator() 末尾会调用
  // updateTimerState()，那时必须已有 timer_
  timer_ = new QTimer(this);
  connect(timer_, &QTimer::timeout, this, [this]() {
    // isAnimated() 可能被子类的状态变化改掉而没人通知，这里兜一道：
    // 已经不需要动画就停下，别让定时器空转（同 ProgressBase）
    if (!animator_ || !animator_->isAnimated()) {
      timer_->stop();
      return;
    }
    animator_->tick();
    update();
  });

  // 默认策略：完整的角色动画。想换成静态 / 无动画 / 实验性策略，
  // 由装配层再 setAnimator() 覆盖
  setAnimator(new DefaultCharacterAnimator());
}

CharacterPanel::~CharacterPanel() {
  // animator_ 已 setParent(this)，QObject 会随本控件一并销毁，这里不
  // 手动 delete。reset 一次是为了给策略一个正规出口，让它在被销毁前
  // 主动作废自己排的延迟任务（眨眼、偷看、错误恢复）
  if (animator_) {
    animator_->reset();
  }
}

void CharacterPanel::setAnimator(CharacterAnimator* animator) {
  if (animator_ == animator) {
    return;
  }
  if (animator_) {
    // 先让旧策略作废自己排的延迟任务，再销毁
    animator_->reset();
    delete animator_;  // 会先从孩子列表摘出自己，安全
  }
  animator_ = animator;
  if (animator_) {
    animator_->setParent(this);  // 随本控件一起销毁
    // 策略通过这个信号拉重绘，面板不再需要去猜谁改了画面
    connect(animator_, &CharacterAnimator::repaintRequested, this,
            qOverload<>(&QWidget::update));
    // 帧率归策略定（如默认角色动画要 16ms 才顺），换策略即换节奏
    timer_->setInterval(animator_->animationIntervalMs());
    animator_->setInput(input_);  // 立即同步当前状态
  }
  updateTimerState();
  update();
}

void CharacterPanel::setTyping(bool on) {
  input_.typing = on;
  pushInput();
}

void CharacterPanel::setPasswordFocused(bool on) {
  input_.password_focused = on;
  pushInput();
}

void CharacterPanel::setPasswordVisible(bool on) {
  input_.password_visible = on;
  pushInput();
}

void CharacterPanel::setHasPassword(bool on) {
  input_.has_password = on;
  pushInput();
}

void CharacterPanel::triggerError() {
  if (animator_) {
    animator_->onValidationFailed();
  }
}

void CharacterPanel::pushInput() {
  if (animator_) {
    animator_->setInput(input_);
  }
  // 输入变化可能让策略进出"需要动画"的状态，所以每次输入都重估一次
  // 定时器（同 ProgressBase 在 setMinimum/setMaximum 里处理忙碌态）
  updateTimerState();
}

void CharacterPanel::updateTimerState() {
  const bool should_run = isVisible() && animator_ && animator_->isAnimated();
  if (should_run) {
    if (!timer_->isActive()) {
      timer_->start();
    }
  } else {
    timer_->stop();
  }
}

void CharacterPanel::paintEvent(QPaintEvent*) {
  if (!animator_) {
    return;
  }
  QPainter p(this);
  animator_->paint(p, rect());
}

void CharacterPanel::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  input_.panel_size = size();
  pushInput();
}

void CharacterPanel::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  updateTimerState();
}

void CharacterPanel::hideEvent(QHideEvent* event) {
  QWidget::hideEvent(event);
  updateTimerState();
}

bool CharacterPanel::eventFilter(QObject*, QEvent* event) {
  // 抓全局鼠标位置，写进 Input 再推给策略。控制器只做"把坐标从全局
  // 换算到本控件坐标系"这一件事，跟随与否、跟随时机由策略自己决定。
  // 这是应用级过滤器，每条 MouseMove 都会到这儿：面板不可见直接放行，
  // 坐标没变不重推，把开销压到"真的在看"的那部分
  if (event->type() == QEvent::MouseMove) {
    if (!isVisible()) {
      return false;
    }
    auto* me = static_cast<QMouseEvent*>(event);
    const QPointF pos = mapFromGlobal(me->globalPos());
    if (pos == input_.mouse_local) {
      return false;
    }
    input_.mouse_local = pos;
    pushInput();
  }
  return false;
}