#ifndef FILE_APP_DIALOGS_LOGIN_ASCII_TV_STATIC_ANIMATOR_H_
#define FILE_APP_DIALOGS_LOGIN_ASCII_TV_STATIC_ANIMATOR_H_

#include "dialogs/login/ascii/ascii_scene_animator.h"
#include "dialogs/login/ascii/tv_static.h"

// 电视的交互策略：把登录窗的交互翻译成"信号质量"。
//   typing          -> 卡片溶进雪花、被撕裂打断（不滚）
//   passwordFocused -> 轻微失锁 + 慢跑
//   passwordVisible -> 调谐锁定 800ms（露测试卡），密码为空不触发
//   validationFailed-> 强干扰 2500ms（雪暴 + 碎卡错位 + 滚屏）
//
// 两个倒计时走 tick() 的帧计数（面板不可见时定时器自停，倒计时随之挂起），
// 连点不会叠链。详细数值见 docs/plan/电视动画交互化设计.md。
class TvStaticAnimator : public AsciiSceneAnimator<TvStatic> {
 public:
  explicit TvStaticAnimator(QRgb background, QObject* parent = nullptr)
      : AsciiSceneAnimator<TvStatic>(background, parent) {}

  void setInput(const Input& in) override;
  void onValidationFailed() override;
  void tick() override;
  void reset() override;

 private:
  // 当前档位（错误 > 偷看 > 打字 > 聚焦，短路取第一个成立者）
  struct Mode {
    float lock_override = -1.0f;  // -1 = 不覆盖，走循环自己的 lock
    float roll_per_frame = 0.0f;  // 行/帧；0 表示这一档不滚
    bool uses_phase = false;      // 是否把累计相位放进画面
  };
  Mode CurrentMode() const;
  void ApplyControl();

  int error_frames_ = 0;  // 剩余帧数（50ms 一帧）
  int peek_frames_ = 0;
  float roll_extra_ = 0.0f;
  bool prev_gate_ = false;  // 上一帧的"密码可见且非空"，用来判上升沿
};

#endif  // FILE_APP_DIALOGS_LOGIN_ASCII_TV_STATIC_ANIMATOR_H_
