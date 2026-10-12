#include <QApplication>

#ifdef APP_UI_QSS
#include <style.h>
#endif

#include "fft_demo_window.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);

#ifdef APP_UI_QSS
  // 底座层样式表（本示例没有项目层，看到的就是纯底座）
  ApplyStyleSheet(app);
#endif

  FftDemoWindow window;
  window.show();

  return app.exec();
}
