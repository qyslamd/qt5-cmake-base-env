// ============================================================================
//  AsciiDotView — 把 (step, colorIdx) 画成圆点字形
//
//  step 0/1/2/3 分别对应空、·、•、● 的视觉等效：
//    直径 = 0%、30%、58%、90% 的格子宽度
//
//  和 AsciiGLView 相比：
//    - 不需要 OpenGL，纯 QPainter
//    - 每格是一个圆点，不是一整块方像素
//    - 缩放交给 QPainter 变换，圆点永远是圆
// ============================================================================
#ifndef EXAMPLES_JUST_FOR_FUN_ASCII_DOT_VIEW_H_
#define EXAMPLES_JUST_FOR_FUN_ASCII_DOT_VIEW_H_

#include <QColor>
#include <QElapsedTimer>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

template <typename Scene>
class AsciiDotView : public QWidget {
 public:
  explicit AsciiDotView(QRgb background, QWidget* parent = nullptr)
      : QWidget(parent), bg_(background) {
    initUi();
    initSignals();

    // 调色板 → QColor
    const char* const* hex_arr = Scene::PaletteHex();
    for (int i = 0; i < Scene::kPaletteN; ++i) {
      palette_colors_[i] = QColor(hex_arr[i]);
    }

    BuildGlyphs();

    // 出第一帧
    scene_.Update(0.0);

    clock_.start();
    timer_.setInterval(1000 / 15);
    timer_.start();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.fillRect(rect(), bg_);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 逻辑尺寸 = 200*16 × 100*16 = 3200 × 1600，宽高比 2:1
    const qreal logicalW = Scene::kWidth * kCell;
    const qreal logicalH = Scene::kHeight * kCell;

    // 等比缩放，居中
    const qreal scale =
        std::min(qreal(width()) / logicalW, qreal(height()) / logicalH);
    const qreal ox = (width() - logicalW * scale) * 0.5;
    const qreal oy = (height() - logicalH * scale) * 0.5;
    p.translate(ox, oy);
    p.scale(scale, scale);

    const auto& step = scene_.stepBuf();
    const auto& color = scene_.colorBuf();

    for (int r = 0; r < Scene::kHeight; ++r) {
      const int base = r * Scene::kWidth;
      const int yy = r * kCell;
      for (int x = 0; x < Scene::kWidth; ++x) {
        const int s = step[base + x];
        if (s == 0)
          continue;
        const int c = color[base + x];
        p.drawPixmap(x * kCell, yy, glyphs_[s][c]);
      }
    }
  }

 private:
  void initUi() {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    resize(Scene::kWidth * kCell, Scene::kHeight * kCell);
  }

  void initSignals() {
    connect(&timer_, &QTimer::timeout, this, &AsciiDotView::Tick);
  }

  void Tick() {
    const double t = clock_.elapsed() / 1000.0;
    scene_.Update(t);
    update();
  }

  // 预渲染 4 × kPaletteN 个圆点
  void BuildGlyphs() {
    // 每个 step 的圆直径，相对格子宽度
    static constexpr float kDiam[4] = {0.0f, 0.30f, 0.58f, 0.90f};

    for (int s = 0; s < 4; ++s) {
      const qreal radius = kDiam[s] * kCell * 0.5;
      for (int c = 0; c < Scene::kPaletteN; ++c) {
        QPixmap pm(kCell, kCell);
        pm.fill(Qt::transparent);
        if (s > 0) {
          QPainter p(&pm);
          p.setRenderHint(QPainter::Antialiasing, true);
          p.setBrush(palette_colors_[c]);
          p.setPen(Qt::NoPen);
          p.drawEllipse(QPointF(kCell * 0.5, kCell * 0.5), radius, radius);
        }
        glyphs_[s][c] = pm;
      }
    }
  }

  // kCell 只是逻辑网格的边长。真正显示尺寸由 paintEvent 的 scale 决定。
  // 调大它可以让低分辨率下的圆更平滑；内存可以忽略不计。
  static constexpr int kCell = 16;

  Scene scene_;
  QTimer timer_;
  QElapsedTimer clock_;

  QColor bg_;
  QColor palette_colors_[Scene::kPaletteN];
  QPixmap glyphs_[4][Scene::kPaletteN];
};

#endif  // ASCII_DOT_VIEW_H