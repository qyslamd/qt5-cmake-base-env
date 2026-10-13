#include <QApplication>
#include <QFont>

#ifdef APP_UI_QSS
#include <style.h>  // 只在 qss 路线编得到
#else
#include <fluentui3style.h>  // 只在 fluent 路线编得到
#endif

#include "login_window.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

#ifdef APP_UI_QSS
  ApplyStyleSheet(app, {QStringLiteral(":/new_login_dialog/login_window.qss")});
#else
  app.setStyle(new FluentUI3Style);

  // FluentUI3Style 不管应用字体，这里补上（qss 路线由 ApplyStyleSheet 设）
  QFont app_font = app.font();
  app_font.setFamily(QStringLiteral("Microsoft YaHei"));
  app.setFont(app_font);
#endif

  LoginWindow w;
  w.show();
  return app.exec();
}
