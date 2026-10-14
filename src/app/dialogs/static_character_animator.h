#ifndef FILE_APP_DIALOGS_STATIC_CHARACTER_ANIMATOR_H_
#define FILE_APP_DIALOGS_STATIC_CHARACTER_ANIMATOR_H_

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QStringList>
#include <QVector>

#include "character_animator.h"

// 静态策略：同样四个角色、同样的背景与 logo，但一动不动。
// isAnimated() 继承默认的 false —— 面板定时器一次都不会启动，
// 内部也没有 16ms 定时器、没有 singleShot、没有相位。
//
// 它的存在同时说明一件事：把"有没有动画"落实成策略，而不是
// 面板上的 bool 开关。切到这一支之后，面板与窗口代码零改动。
class StaticCharacterAnimator : public CharacterAnimator {
  Q_OBJECT

 public:
  explicit StaticCharacterAnimator(QObject* parent = nullptr);

  // 静态版对 Input 不敏感，但尺寸还是要的——场景位置靠它换算
  void setInput(const Input& in) override;

  // isAnimated() 用基类的 false；tick()/reset() 用基类的空实现
  void paint(QPainter& p, const QRectF& rect) const override;

 private:
  struct StaticChar {
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
  };

  void initChars();
  void updateScene(const QSize& panel_size);

  void drawBackground(QPainter& p, const QRectF& rect) const;
  void drawLogo(QPainter& p) const;
  void drawFooter(QPainter& p, const QRectF& rect) const;
  void drawChar(QPainter& p, const StaticChar& c) const;

  QVector<StaticChar> chars_;
  QRectF scene_;
  QString logo_text_ = QStringLiteral("CareerCompass");
  QStringList footer_links_{QStringLiteral("帮助"), QStringLiteral("关于")};
};

#endif  // FILE_APP_DIALOGS_STATIC_CHARACTER_ANIMATOR_H_