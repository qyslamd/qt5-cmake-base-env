#include <QApplication>

#include "gallery_window.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("libui_gallery"));

  GalleryWindow window;
  window.show();

  return app.exec();
}
