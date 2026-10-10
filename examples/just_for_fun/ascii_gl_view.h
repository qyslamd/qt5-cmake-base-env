// ============================================================================
//  AsciiGLView — 把 ASCII/半色调场景用 OpenGL 放大到窗口
//
//  场景每帧产出 200×100 的 (step, colorIdx) 缓冲，CPU 侧打包成 RGBA8 位图，
//  上传成纹理，用 GL_NEAREST 绘制到全屏四边形。放大由 GPU 做，画质是干净的
//  像素块，正是半色调该有的样子。
//
//  模板参数 Scene 需要提供：
//      static constexpr int kWidth, kHeight, kPaletteN;
//      void Update(double t);
//      const std::vector<uint8_t>& stepBuf()  const;
//      const std::vector<uint8_t>& colorBuf() const;
//      static const char* const* PaletteHex();
// ============================================================================
#ifndef EXAMPLES_JUST_FOR_FUN_ASCII_GL_VIEW_H_
#define EXAMPLES_JUST_FOR_FUN_ASCII_GL_VIEW_H_

#include <array>

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QTimer>
#include <QVector2D>
#include <QVector3D>

template <typename Scene>
class AsciiGLView : public QOpenGLWidget, protected QOpenGLFunctions {
 public:
  explicit AsciiGLView(QRgb background, QWidget* parent = nullptr)
      : QOpenGLWidget(parent), bg_(background) {
    initUi();
    initSignals();

    // 预解析调色板
    const char* const* hex_arr = Scene::PaletteHex();
    for (int i = 0; i < Scene::kPaletteN; ++i) {
      palette_[i] = QColor(hex_arr[i]).rgb();
    }
    // CPU 侧缓冲：Format_RGBA8888 直接对应 GL_RGBA
    cpu_image_ = QImage(Scene::kWidth, Scene::kHeight, QImage::Format_RGBA8888);
    cpu_image_.fill(bg_);

    // 场景初始化（alpine dawn 大约 2–4 秒）
    scene_.Update(0.0);
    RebuildCpuImage();

    clock_.start();

    // 定时器在 initializeGL 里启动，避免 GL 就绪前触发绘制
    timer_.setInterval(1000 / 15);
  }

  ~AsciiGLView() override {
    // 必须在上下文当前的情况下释放 GL 资源
    makeCurrent();
    if (texture_) {
      glDeleteTextures(1, &texture_);
      texture_ = 0;
    }
    vbo_.destroy();
    doneCurrent();
  }

 protected:
  void initializeGL() override {
    initializeOpenGLFunctions();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // --- 着色器 -------------------------------------------------------
    static const char* kVertSrc = R"(
#version 120
attribute vec2 aPos;
attribute vec2 aUV;
varying   vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";
    static const char* kFragSrc = R"(
#version 120
uniform sampler2D uTex;
uniform vec3 uBg;      // 背景色
uniform vec2 uCells;   // (kWidth, kHeight)
varying vec2 vUV;

void main() {
    vec4 tex = texture2D(uTex, vUV);

    // 还原 step：0/1/2/3
    int s = int(tex.a * 3.0 + 0.5);

    if (s == 0) {
        gl_FragColor = vec4(uBg, 1.0);
        return;
    }

    // cell 内归一化坐标 [-0.5, 0.5]
    vec2 cell = vUV * uCells;
    vec2 f = fract(cell) - vec2(0.5);

    // 直径与 QPainter 的 kDiam 一致：0%, 30%, 58%, 90%
    float diam = 0.0;
    if (s == 1) diam = 0.30;
    else if (s == 2) diam = 0.58;
    else if (s == 3) diam = 0.90;
    float r = diam * 0.5;

    float dist = length(f);

    // 抗锯齿：约 1 屏幕像素对应的 cell 单位
    float aa = fwidth(cell.x) * 1.2;
    float mask = 1.0 - smoothstep(r - aa, r + aa, dist);

    vec3 col = mix(uBg, tex.rgb, mask);
    gl_FragColor = vec4(col, 1.0);
}
)";

    program_.addShaderFromSourceCode(QOpenGLShader::Vertex, kVertSrc);
    program_.addShaderFromSourceCode(QOpenGLShader::Fragment, kFragSrc);
    program_.bindAttributeLocation("aPos", 0);
    program_.bindAttributeLocation("aUV", 1);
    if (!program_.link()) {
      qWarning("AsciiGLView: shader link failed: %s",
               qPrintable(program_.log()));
    }

    // --- 全屏四边形（三角带） ----------------------------------------
    //  屏幕坐标          纹理坐标
    //  (-1,-1) 左下       (0,1)
    //  ( 1,-1) 右下       (1,1)
    //  (-1, 1) 左上       (0,0)
    //  ( 1, 1) 右上       (1,0)
    //  QImage 第 0 行 → v=0 → 屏幕顶部，朝向一致
    static const GLfloat kVerts[] = {
        -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f,  1.0f, 0.0f,
    };
    vbo_.create();
    vbo_.bind();
    vbo_.setUsagePattern(QOpenGLBuffer::StaticDraw);
    vbo_.allocate(kVerts, sizeof(kVerts));
    vbo_.release();

    // --- 纹理 ---------------------------------------------------------
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // 一次性分配存储，之后用 TexSubImage 更新
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Scene::kWidth, Scene::kHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    gl_ready_ = true;
    timer_.start();
  }
  void resizeGL(int w, int h) override {
    // 保持 200:100 = 2:1 的纵横比，居中留黑
    const qreal logicalAR = qreal(Scene::kWidth) / Scene::kHeight;  // 2.0
    const qreal windowAR = qreal(w) / h;

    int vpW, vpH, vpX, vpY;
    if (windowAR > logicalAR) {
      // 窗口太宽：左右留黑
      vpH = h;
      vpW = int(h * logicalAR);
      vpX = (w - vpW) / 2;
      vpY = 0;
    } else {
      // 窗口太高：上下留黑
      vpW = w;
      vpH = int(w / logicalAR);
      vpX = 0;
      vpY = (h - vpH) / 2;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glViewport(vpX, vpY, vpW, vpH);
  }

  void paintGL() override {
    if (!gl_ready_) {
      return;
    }

    glClear(GL_COLOR_BUFFER_BIT);

    // 上传这一帧的像素
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Scene::kWidth, Scene::kHeight,
                    GL_RGBA, GL_UNSIGNED_BYTE, cpu_image_.constBits());

    // 绘制
    program_.bind();
    program_.setUniformValue("uTex", 0);
    program_.setUniformValue("uBg", float(qRed(bg_)) / 255.0f,
                             float(qGreen(bg_)) / 255.0f,
                             float(qBlue(bg_)) / 255.0f);
    program_.setUniformValue("uCells", float(Scene::kWidth),
                             float(Scene::kHeight));

    vbo_.bind();
    program_.enableAttributeArray(0);
    program_.enableAttributeArray(1);
    program_.setAttributeBuffer(0, GL_FLOAT, 0, 2,
                                4 * sizeof(GLfloat));  // aPos
    program_.setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(GLfloat), 2,
                                4 * sizeof(GLfloat));  // aUV

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    program_.disableAttributeArray(0);
    program_.disableAttributeArray(1);
    vbo_.release();
    program_.release();

    glBindTexture(GL_TEXTURE_2D, 0);
  }

 private:
  void initUi() {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    resize(Scene::kWidth * 3, Scene::kHeight * 3);
  }

  void initSignals() {
    connect(&timer_, &QTimer::timeout, this, &AsciiGLView::Tick);
  }

  void Tick() {
    double t = clock_.elapsed() / 1000.0;
    scene_.Update(t);
    RebuildCpuImage();
    update();  // 请求重绘 → 下一帧 paintGL
  }

  // 把 (step, colorIdx) 打包成 RGBA8
  void RebuildCpuImage() {
    const auto& step = scene_.stepBuf();
    const auto& color = scene_.colorBuf();

    for (int r = 0; r < Scene::kHeight; ++r) {
      uchar* line = cpu_image_.scanLine(r);
      const int base = r * Scene::kWidth;
      for (int x = 0; x < Scene::kWidth; ++x) {
        const int k = base + x;
        const int s = step[k];
        uchar* px = line + x * 4;
        if (s == 0) {
          // 背景：rgb 随便填，shader 会用 uBg
          px[0] = px[1] = px[2] = 0;
          px[3] = 0;
        } else {
          const QRgb c = palette_[color[k]];
          px[0] = uchar(qRed(c));
          px[1] = uchar(qGreen(c));
          px[2] = uchar(qBlue(c));
          px[3] = uchar(s * 85);  // 1→85, 2→170, 3→255
        }
      }
    }
  }

  // ---- 场景状态 --------------------------------------------------------
  Scene scene_;
  QTimer timer_;
  QElapsedTimer clock_;

  // ---- GL 资源 ---------------------------------------------------------
  QOpenGLShaderProgram program_;
  QOpenGLBuffer vbo_{QOpenGLBuffer::VertexBuffer};
  GLuint texture_ = 0;
  bool gl_ready_ = false;

  // ---- CPU 侧 ----------------------------------------------------------
  QImage cpu_image_;
  QRgb palette_[Scene::kPaletteN];
  QRgb bg_;
};

#endif  // ASCII_GL_VIEW_H