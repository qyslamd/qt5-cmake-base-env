#include "tour_style.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QString>
#include <QtDebug>

namespace {

const char kStyleSheetPath[] = ":/resources/styles/tour_default.qss";

QString ReadStyleSheetText() {
  QFile file(QString::fromLatin1(kStyleSheetPath));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString();
  }
  return QString::fromUtf8(file.readAll());
}

}  // namespace

// 静态库里的 qrc 符号没人引用就不会被链进来，这里是那个引用。
//
// 单独放一个全局函数、不放进匿名命名空间也不是类成员：Q_INIT_RESOURCE 展开
// 后会声明并调用 qInitResources_tour_res()，那个函数由 rcc 生成在全局作用
// 域——放进命名空间会去找一个不存在的 ns::qInitResources_tour_res
static void InitTourResources() { Q_INIT_RESOURCE(tour_res); }

void TourStyle::EnsureApplied() {
  static bool applied = false;
  if (applied) {
    return;
  }

  InitTourResources();

  QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
  if (app == nullptr) {
    // 应用对象还没建起来。**这一趟什么也别记**——applied 一旦置真，后面每次
    // 调用都会在这里直接返回，样式就永远不生效，而且不报错
    return;
  }

  const QString own = ReadStyleSheetText();
  if (own.isEmpty()) {
    qWarning() << "引导样式表读取失败，引导界面将无样式：" << kStyleSheetPath;
    return;  // 同理：读失败不记账，下次调用还能再试一次
  }

  // 追加而非覆盖：宿主可能已经设过自己的样式表。
  //
  // 之所以设在应用级、而不是引导自己那棵子树上（那边有同名注释）：实操模式下
  // 气泡会被 reparent 到 viewport，离开首页的子树，只设在首页上盖不到它。
  // 代价是这份表与"谁后设应用样式表"有顺序依赖——本仓的顺序是 main.cpp 先设
  // （底座层 + 项目层）、这里后追加，不要再往中间插别处设应用样式表
  app->setStyleSheet(app->styleSheet() + QLatin1Char('\n') + own);
  // 只有真设进去了才算数（见上面两个 early return 的注释）
  applied = true;
}
