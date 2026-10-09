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

// 静态库里的 qrc 符号没人引用就不会被链进来，这里是那个引用。**必须在全局
// 作用域**：Q_INIT_RESOURCE 展开后调的是 rcc 生成的全局函数，放进命名空间会去
// 找不存在的 ns:: 版本
static void InitTourResources() { Q_INIT_RESOURCE(tour_res); }

void TourStyle::ensureApplied() {
  static bool applied = false;
  if (applied) {
    return;
  }

  InitTourResources();

  QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
  if (app == nullptr) {
    // **这一趟什么也别记**：applied 一旦置真，后面每次调用都会从这里返回，
    // 样式永远不生效、而且不报错
    return;
  }

  const QString own = ReadStyleSheetText();
  if (own.isEmpty()) {
    qWarning() << "引导样式表读取失败，引导界面将无样式：" << kStyleSheetPath;
    return;  // 同理：读失败不记账，下次调用还能再试一次
  }

  // 追加而非覆盖：宿主可能已经设过自己的样式表。副作用是它必须排在宿主样式表
  // 之后（本仓是 main.cpp 先设底座层与项目层、这里后追加），不要再往中间插
  // 别处设应用样式表
  app->setStyleSheet(app->styleSheet() + QLatin1Char('\n') + own);
  // 只有真设进去了才算数（见上面两个 early return 的注释）
  applied = true;
}
