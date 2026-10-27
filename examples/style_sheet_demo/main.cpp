#include <QApplication>

#include "style_sheet.h"
#include "style_sheet_demo_window.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

#ifndef APP_UI_FLUENT
  ApplyStyleSheet(app);
#endif

  StyleSheetDemoWindow window;
  window.show();
  return app.exec();
}