// ============================================================================
//  TextView — 把 (QChar, colorIdx) 用等宽字体逐格画
//
//  Scene 接口：
//    static constexpr int kWidth, kHeight, kPaletteN;
//    static const char* const* PaletteHex();
//    void Update(double t);
//    const std::vector<QChar>&    charBuf()  const;
//    const std::vector<uint8_t>& colorBuf() const;
// ============================================================================
#ifndef EXAMPLES_JUST_FOR_FUN_TEXT_VIEW_H_
#define EXAMPLES_JUST_FOR_FUN_TEXT_VIEW_H_

#include <algorithm>

#include <QColor>
#include <QElapsedTimer>
#include <QFont>
#include <QFontDatabase>
#include <QPainter>
#include <QTimer>
#include <QWidget>

template <typename Scene>
class TextView : public QWidget {
 public:
  explicit TextView(QRgb background, QWidget* parent = nullptr)
      : QWidget(parent), bg_(background) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);

    const char* const* hex_arr = Scene::PaletteHex();
    for (int i = 0; i < Scene::kPaletteN; ++i)
      palette_colors_[i] = QColor(hex_arr[i]);

    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setStyleHint(QFont::Monospace);
    base_font_ = f;

    scene_.Update(0.0);
    clock_.start();
    timer_.setInterval(1000 / 20);
    timer_.start();
    connect(&timer_, &QTimer::timeout, this, [this] {
      scene_.Update(clock_.elapsed() / 1000.0);
      update();
    });
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.fillRect(rect(), bg_);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // 逻辑格：宽 1，高 2（等宽字体高约是宽的两倍）
    const qreal cellW = 1.0;
    const qreal cellH = 2.0;
    const qreal logicalW = Scene::kWidth * cellW;
    const qreal logicalH = Scene::kHeight * cellH;

    const qreal scale =
        std::min(qreal(width()) / logicalW, qreal(height()) / logicalH);
    const qreal ox = (width() - logicalW * scale) * 0.5;
    const qreal oy = (height() - logicalH * scale) * 0.5;

    QFont f = base_font_;
    f.setPixelSize(std::max(4, int(cellH * scale * 0.98)));
    p.setFont(f);

    const auto& chars = scene_.charBuf();
    const auto& colors = scene_.colorBuf();

    for (int r = 0; r < Scene::kHeight; ++r) {
      const int base = r * Scene::kWidth;
      const qreal y = oy + r * cellH * scale;
      for (int c = 0; c < Scene::kWidth; ++c) {
        const QChar ch = chars[base + c];
        if (ch == QChar(' '))
          continue;
        const qreal x = ox + c * cellW * scale;
        p.setPen(palette_colors_[colors[base + c]]);
        p.drawText(QRectF(x, y, cellW * scale, cellH * scale), Qt::AlignCenter,
                   QString(ch));
      }
    }
  }

 private:
  Scene scene_;
  QTimer timer_;
  QElapsedTimer clock_;

  QRgb bg_;
  QColor palette_colors_[Scene::kPaletteN];
  QFont base_font_;
};

#endif  // EXAMPLES_JUST_FOR_FUN_TEXT_VIEW_H_