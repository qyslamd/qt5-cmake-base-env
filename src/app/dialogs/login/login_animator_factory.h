#ifndef FILE_APP_DIALOGS_LOGIN_LOGIN_ANIMATOR_FACTORY_H_
#define FILE_APP_DIALOGS_LOGIN_LOGIN_ANIMATOR_FACTORY_H_

class CharacterAnimator;

// 按配置 login/animation 造左栏动画策略：default（四角色动效）/ static
// （同款静止）/ tv（电视雪花）/ rocket（火箭发射）。未知值回落 default
// 并记一条 WARN。返回值归调用方，交给面板接管所有权
CharacterAnimator* CreateLoginAnimator();

#endif  // FILE_APP_DIALOGS_LOGIN_LOGIN_ANIMATOR_FACTORY_H_
