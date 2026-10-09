#include <QApplication>

#include "demo_widget.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);

  DemoWidget window;
  window.show();

  return app.exec();
}
