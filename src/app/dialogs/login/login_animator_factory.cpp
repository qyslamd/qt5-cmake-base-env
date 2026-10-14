#include "login_animator_factory.h"

#include <QRgb>

#include "config/config_defs.h"
#include "config/config_manager.h"
#include "dialogs/login/ascii/ascii_scene_animator.h"
#include "dialogs/login/ascii/rocket.h"
#include "dialogs/login/ascii/tv_static.h"
#include "dialogs/login/character_animator.h"
#include "dialogs/login/default_character_animator.h"
#include "dialogs/login/static_character_animator.h"
#include "logger/logger.h"

CharacterAnimator* CreateLoginAnimator() {
  // 缺省值必须传 QString：get<T> 走 QVariant::fromValue，传字面量会推导成
  // char[] 命中未注册类型的 static_assert
  const QString name = ConfigManager::instance().get<QString>(
      QString::fromLatin1(kConfigLoginAnimation),
      QString::fromLatin1(kLoginAnimationDefault));

  if (name == QLatin1String(kLoginAnimationStatic)) {
    return new StaticCharacterAnimator;
  }
  if (name == QLatin1String(kLoginAnimationTv)) {
    // 底色同 qt-libui 示例的注册值
    return new AsciiSceneAnimator<TvStatic>(qRgb(0x0b, 0x08, 0x14));
  }
  if (name == QLatin1String(kLoginAnimationRocket)) {
    return new AsciiSceneAnimator<Rocket>(qRgb(0x00, 0x00, 0x00));
  }
  if (name != QLatin1String(kLoginAnimationDefault)) {
    LOG_WARN("LOGIN", "未知的 login/animation 取值 {}，回落默认角色动效",
             name.toStdString());
  }
  return new DefaultCharacterAnimator;
}
