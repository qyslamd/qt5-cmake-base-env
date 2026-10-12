#include <QApplication>

#include "fft_demo_window.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);

  FftDemoWindow window;
  window.show();

  return app.exec();
}
