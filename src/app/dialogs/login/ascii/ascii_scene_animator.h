#ifndef FILE_APP_DIALOGS_ASCII_ASCII_SCENE_ANIMATOR_H_
#define FILE_APP_DIALOGS_ASCII_ASCII_SCENE_ANIMATOR_H_

#include <algorithm>

#include <QChar>
#include <QColor>
#include <QElapsedTimer>
#include <QFont>
#include <QFontDatabase>
#include <QPainter>
#include <QRectF>

#include "dialogs/login/character_animator.h"

// 字符场景策略：把 "QChar + 色号" 缓冲用等宽字体逐格画出来。
//
// 场景（模板参数）的约定与原 TextView 一致：
//   static constexpr kWidth / kHeight / kPaletteN
//   static PaletteHex()、Update(double t)、charBuf()、colorBuf()
//
// 底色是构造参数（场景类自己不扛样式）。本类不声明 Q_OBJECT：模板类
// moc 不了，而面板是通过基类指针 connect 的，用不着子类元对象。
// 代价：这个类型只当 CharacterAnimator* 用，qobject_cast / findChild
// 对它不可用；真要找，退化成两个具体类即可。
template <typename Scene>
class AsciiSceneAnimator : public CharacterAnimator {
 public:
  explicit AsciiSceneAnimator(QRgb background, QObject* parent = nullptr)
      : CharacterAnimator(parent), bg_(background) {
    const char* const* hex = Scene::PaletteHex();
    for (int i = 0; i < Scene::kPaletteN; ++i) {
      palette_[i] = QColor(QString::fromLatin1(hex[i]));
    }
    // 首帧兜底：面板装好就立刻 update()，没 Update 过会画出一屏空格
    scene_.Update(0.0);
    elapsed_.start();
  }

  bool isAnimated() const override { return true; }

  // 20fps，同原 TextView；字符量比角色动效大，不必用 16ms
  int animationIntervalMs() const override { return 50; }

  void tick() override { scene_.Update(elapsed_.elapsed() / 1000.0); }

  void paint(QPainter& p, const QRectF& rect) const override {
    p.fillRect(rect, bg_);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // 逻辑格：宽 1、高 2（等宽字体的高约是宽的两倍）
    constexpr qreal kCellW = 1.0;
    constexpr qreal kCellH = 2.0;
    const qreal logical_w = Scene::kWidth * kCellW;
    const qreal logical_h = Scene::kHeight * kCellH;

    const qreal scale =
        std::min(rect.width() / logical_w, rect.height() / logical_h);
    const qreal ox = rect.left() + (rect.width() - logical_w * scale) * 0.5;
    const qreal oy = rect.top() + (rect.height() - logical_h * scale) * 0.5;

    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(std::max(4, int(kCellH * scale * 0.98)));
    p.setFont(f);

    const auto& chars = scene_.charBuf();
    const auto& colors = scene_.colorBuf();
    for (int r = 0; r < Scene::kHeight; ++r) {
      const int base = r * Scene::kWidth;
      const qreal y = oy + r * kCellH * scale;
      for (int c = 0; c < Scene::kWidth; ++c) {
        const QChar ch = chars[base + c];
        if (ch == QChar(' ')) {
          continue;
        }
        const qreal x = ox + c * kCellW * scale;
        p.setPen(palette_[colors[base + c]]);
        p.drawText(QRectF(x, y, kCellW * scale, kCellH * scale),
                   Qt::AlignCenter, QString(ch));
      }
    }
  }

  // 不覆写 reset()：面板只在"换掉本策略"与析构时调它，那时本对象随即销毁，
  // 没有挂起任务要作废；动 elapsed_ 反而会留下未启动的计时器

 private:
  Scene scene_;
  QElapsedTimer elapsed_;
  QRgb bg_;
  QColor palette_[Scene::kPaletteN];
};

#endif  // FILE_APP_DIALOGS_ASCII_ASCII_SCENE_ANIMATOR_H_
