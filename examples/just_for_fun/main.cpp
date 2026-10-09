#include <QApplication>
#include <QHBoxLayout>
#include <QWidget>

#include "alpine_dawn.h"
#include "ascii_dot_view.h"
#include "ascii_gl_view.h"
#include "marine_drive.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);

  // --gl 换用 OpenGL 最近邻放大（干净的像素块）；默认是 QPainter 圆点字形
  const bool use_gl = app.arguments().contains(QStringLiteral("--gl"));

  QWidget root;
  root.setWindowTitle(use_gl ? QStringLiteral("ascii.rest - gl")
                             : QStringLiteral("ascii.rest - dots"));
  auto* layout = new QHBoxLayout(&root);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);
  if (use_gl) {
    layout->addWidget(new AsciiGLView<MarineDrive>(qRgb(0x07, 0x08, 0x0f)));
    layout->addWidget(new AsciiGLView<AlpineDawn>(qRgb(0x09, 0x0c, 0x18)));
  } else {
    layout->addWidget(new AsciiDotView<MarineDrive>(qRgb(0x07, 0x08, 0x0f)));
    layout->addWidget(new AsciiDotView<AlpineDawn>(qRgb(0x09, 0x0c, 0x18)));
  }

  root.resize(1202, 600);
  root.show();

  return app.exec();
}
