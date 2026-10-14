#include "tv_static_animator.h"

#include <cmath>

namespace {

// 帧长与两段时长（都是 50ms 的整数倍：16 帧 / 50 帧）
constexpr int kFrameMs = 50;
constexpr int kPeekFrames = 800 / kFrameMs;
constexpr int kErrorFrames = 2500 / kFrameMs;

// 各档的 lock 与滚动速度，见设计文档 §3.4
constexpr float kTypingLock = 0.45f;
constexpr float kFocusLock = 0.80f;
constexpr float kPeekLock = 1.0f;
constexpr float kErrorLock = 0.40f;
constexpr float kFocusRollPerFrame = 0.35f;
constexpr float kErrorRollPerFrame = 0.8f;

// 卡片行周期（kSH + 2），相位在它上面折返
constexpr float kRollPeriod = 17.0f;

}  // namespace

TvStaticAnimator::Mode TvStaticAnimator::CurrentMode() const {
  Mode m;
  if (error_frames_ > 0) {
    m.lock_override = kErrorLock;
    m.roll_per_frame = kErrorRollPerFrame;
    m.uses_phase = true;
  } else if (peek_frames_ > 0) {
    // 偷看是"调谐咬住信号"：锁死并吸附回 roll=0，相位不许进画面
    m.lock_override = kPeekLock;
  } else if (input_.typing) {
    m.lock_override = kTypingLock;
    m.uses_phase = true;
  } else if (input_.password_focused) {
    m.lock_override = kFocusLock;
    m.roll_per_frame = kFocusRollPerFrame;
    m.uses_phase = true;
  }
  return m;
}

void TvStaticAnimator::ApplyControl() {
  const Mode m = CurrentMode();
  TvStatic::TvControl c;
  c.lock_override = m.lock_override;
  c.roll_extra = m.uses_phase ? roll_extra_ : 0.0f;
  scene_.SetControl(c);
}

void TvStaticAnimator::setInput(const Input& in) {
  // 上升沿判定必须在调基类之前：基类的 setInput 会把 input_ 覆盖成 in，
  // 之后再比就永远相等，偷看将静默失效
  const bool gate = in.password_visible && in.has_password;
  if (gate && !prev_gate_) {
    peek_frames_ = kPeekFrames;
  }
  prev_gate_ = gate;

  CharacterAnimator::setInput(in);
  ApplyControl();
}

void TvStaticAnimator::onValidationFailed() {
  error_frames_ = kErrorFrames;
  ApplyControl();
}

void TvStaticAnimator::tick() {
  // 先递减再取档：否则每档多演一帧
  if (error_frames_ > 0) {
    --error_frames_;
  }
  if (peek_frames_ > 0) {
    --peek_frames_;
  }

  const Mode m = CurrentMode();
  if (m.roll_per_frame != 0.0f) {
    // 折返到行周期内，免得长时间累计后 float 精度出量化抖动
    roll_extra_ = std::fmod(roll_extra_ + m.roll_per_frame, kRollPeriod);
  }
  ApplyControl();

  // 控制量必须在这一帧的 Update 之前落进场景（基类 tick 一进门就 Update）
  AsciiSceneAnimator<TvStatic>::tick();
}

void TvStaticAnimator::reset() {
  error_frames_ = 0;
  peek_frames_ = 0;
  roll_extra_ = 0.0f;
  prev_gate_ = false;
  ApplyControl();
}
