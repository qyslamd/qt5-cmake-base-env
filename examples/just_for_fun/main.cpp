#include <style.h>

#include <QApplication>

#include "widget.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);

  //  from static library libstyle
  ApplyStyleSheet(app, {":/just_for_fun.qss"});

  Widget w;
  w.show();

  return app.exec();
}
