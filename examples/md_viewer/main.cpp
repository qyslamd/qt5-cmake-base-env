// Markdown 查看器的入口：观感旋钮照 FluentUIStyle 官方 Gallery 抄。
// 本示例固定 fluent——不加载任何 QSS（样式表一生效就会绕过 Fluent 绘制）
#include <fluentui3style.h>

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QStringList>

#include "md_viewer_window.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  // 图标走 qrc（windeployqt 会带上 qsvg 插件——markdown 模块链了 Qt5::Svg）
  app.setWindowIcon(QIcon(QStringLiteral(":/resources/svg/icon.svg")));

  // 官方 Gallery 的同款旋钮：二级圆角与配色方案（0=Fluent，1=Teams）
  app.setProperty("secondLevelRoundingRadius", 3);
  app.setProperty("_q_themestyle", 0);
  app.setStyle(new FluentUI3Style);

  QFont app_font = app.font();
  app_font.setFamily(QStringLiteral("Microsoft YaHei"));
  app.setFont(app_font);

  MdViewerWindow window;
  window.show();

  // 第一个位置参数：目录就铺开侧栏，文件直接看
  const QStringList args = app.arguments();
  if (args.size() > 1) {
    window.OpenPath(args.at(1));
  }
  return app.exec();
}
