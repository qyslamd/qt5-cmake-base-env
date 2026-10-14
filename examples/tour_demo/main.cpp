#include <QApplication>

#ifndef APP_UI_FLUENT
#include <style_sheet.h>
#endif

#include "demo_widget.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);

#ifndef APP_UI_FLUENT
  // 底座层样式表。引导自己的样式由 TourStyle::ensureApplied() 追加上来
  // （它用 app->styleSheet() + 拼接，先底座后引导的顺序天然成立）
  ApplyStyleSheet(app);
#endif

  DemoWidget window;
  window.show();

  return app.exec();
}
