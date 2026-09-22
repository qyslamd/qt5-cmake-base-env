#include "guidance_style.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QString>
#include <QtDebug>

namespace {

const char kStyleSheetPath[] = ":/resources/styles/guidance_default.qss";

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
// 后会声明并调用 qInitResources_guidance_res()，那个函数由 rcc 生成在全局作用
// 域——放进命名空间会去找一个不存在的 ns::qInitResources_guidance_res
static void InitGuidanceResources() { Q_INIT_RESOURCE(guidance_res); }

void GuidanceStyle::EnsureApplied() {
  static bool applied = false;
  if (applied) {
    return;
  }
  applied = true;

  InitGuidanceResources();

  QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
  if (app == nullptr) {
    return;
  }

  const QString own = ReadStyleSheetText();
  if (own.isEmpty()) {
    qWarning() << "引导样式表读取失败，引导界面将无样式：" << kStyleSheetPath;
    return;
  }

  // 追加而非覆盖：宿主可能已经设过自己的样式表
  app->setStyleSheet(app->styleSheet() + QLatin1Char('\n') + own);
}
