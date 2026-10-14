#include "application.h"

#ifdef APP_UI_FLUENT
#include <fluentui3style.h>
#else
#include <style_sheet.h>
#endif

#include <QFont>
#include <QIcon>
#include <QLibraryInfo>

#include "AppInfo.h"
#include "AppVersion.h"

Application::Application(int& argc, char** argv) : QApplication(argc, argv) {
  setupMetadata();
  setupStyle();
  installTranslations();
}

void Application::setupMetadata() {
  // 组织名/应用名决定数据目录、单实例键与日志文件名，须与安装脚本一致
  setOrganizationName(QString::fromLatin1(APP_ORGANIZATION_NAME));
  setApplicationName(QString::fromLatin1(APP_APPLICATION_NAME));
  setWindowIcon(QIcon(":/resources/icons/icon.ico"));
  setApplicationVersion(getProjectVersionAll());
}

void Application::setupStyle() {
  // 样式两条路线互斥（APP_UI_ROUTE）：fluent 不加载任何 QSS——样式表一生效，
  // 命中控件就走 QStyleSheetStyle，Fluent 绘制被整个绕过；qss 由
  // ApplyStyleSheet 装底座 + 项目两层。全局字体两条路都要设
#ifdef APP_UI_FLUENT
  setStyle(new FluentUI3Style);
#else
  ApplyStyleSheet(*this,
                  {QStringLiteral(":/resources/styles/default.qss"),
                   QStringLiteral(":/resources/styles/login_window.qss")});
#endif
  QFont app_font = font();
  app_font.setFamily(QStringLiteral("Microsoft YaHei"));
  setFont(app_font);
}

void Application::installTranslations() {
  // 界面语言写死：本仓文案全是硬编码中文，要翻的只有第三方控件的自带串，
  // 故不问系统语言、一律 zh_CN——否则中文界面里会混进一段英文
  //
  // Qt 自带控件文案（QMessageBox / QFileDialog 的按钮等）：目录 Windows 侧
  // 由 windeployqt 拷到 exe 旁
  const QString qt_qm = QLibraryInfo::location(QLibraryInfo::TranslationsPath) +
                        QStringLiteral("/qt_zh_CN.qm");
  if (qt_translator_.load(qt_qm)) {
    installTranslator(&qt_translator_);
  } else {
    translation_problems_
        << QStringLiteral("Qt 中文翻译装载失败，标准对话框按钮将显示英文: %1")
               .arg(qt_qm);
  }

  // 功能区文案：构建期从 SARibbon_zh_CN.ts 编出来并嵌进资源；没装 Qt 的
  // LinguistTools 时它不存在，这里只降级、不拦启动
  const QString saribbon_qm =
      QStringLiteral(":/translations/SARibbon_zh_CN.qm");
  if (saribbon_translator_.load(saribbon_qm)) {
    installTranslator(&saribbon_translator_);
  } else {
    translation_problems_ << QStringLiteral(
        "功能区中文翻译装载失败，功能区自带文案将显示英文");
  }
}
