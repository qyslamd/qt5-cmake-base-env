#ifndef FILE_APP_DIALOGS_CHARACTER_ANIMATOR_H_
#define FILE_APP_DIALOGS_CHARACTER_ANIMATOR_H_

#include <QObject>
#include <QPointF>
#include <QSize>

class QPainter;
class QRectF;

// 角色面板的行为策略。只回答"这个面板长什么样、要不要动"，
// 不持有任何控件、不装任何事件过滤器、不认识 LoginWindow。
//
// 面板（CharacterPanel）负责：接收窗口交互、维护 Input、按帧驱动
// tick()、转发 paint()。策略负责：读 Input、维护自己的相位与内部
// 延迟任务、把这一帧画出来。
//
// 生命周期约定：
//   - isAnimated() 返回 false 时，面板不驱动 tick()，但仍会调 paint()。
//     "没有动画"因此不是特例，而是 isAnimated() 恒 false 的策略。
//   - 被替换或面板析构前，会先调 reset()。策略应在此作废自己排的
//     singleShot 链、停掉内部定时器。
class CharacterAnimator : public QObject {
  Q_OBJECT

 public:
  // 面板当前状态。整体替换，不增量
  struct Input {
    QSize panel_size;                     // 面板尺寸，用于算场景位置
    QPointF mouse_local{-10000, -10000};  // 鼠标在面板坐标系里的位置
    bool typing = false;                  // 任一输入框正在编辑
    bool password_focused = false;        // 密码框有焦点
    bool password_visible = false;        // 密码当前可见
    bool has_password = false;            // 密码框非空
  };

  explicit CharacterAnimator(QObject* parent = nullptr) : QObject(parent) {}
  ~CharacterAnimator() override = default;

  // 状态整体替换。策略在此更新自己的字段，并按需重算目标
  virtual void setInput(const Input& in) { input_ = in; }
  const Input& input() const { return input_; }

  // 一次性事件：校验失败（邮箱格式 / 密码长度 / 鉴权失败）
  virtual void onValidationFailed() {}

  // ---- 动画钩子 ----
  // 返回 true 时面板启动逐帧定时器。默认 false：静态策略零开销
  virtual bool isAnimated() const { return false; }
  // 逐帧间隔。只在 isAnimated() 为真时被面板询问——驱动节奏归策略定，
  // 面板不写死帧率
  virtual int animationIntervalMs() const { return 30; }
  // 每帧调用一次，推进自己的相位。只在 isAnimated() 为真时被调用
  virtual void tick() {}

  // 绘制这一帧。rect 是面板的矩形（与 Input::panel_size 对应）
  virtual void paint(QPainter& p, const QRectF& rect) const = 0;

  // 被替换 / 面板析构前调用。作废自己排的延迟任务，停内部定时器
  virtual void reset() {}

 signals:
  void repaintRequested();

 protected:
  Input input_;
};

#endif  // FILE_APP_DIALOGS_CHARACTER_ANIMATOR_H_