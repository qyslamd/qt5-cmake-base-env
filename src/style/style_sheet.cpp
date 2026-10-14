#include "style_sheet.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QFont>
#include <QString>

namespace {

// 底座层的顺序有约束：从宽到窄。QSS 里所有类型选择器具体度相同、后写优先，
// 宽的选择器排在后面会把窄的盖掉（比如 QWidget 的 border 盖掉 QLineEdit 的）。
// 新增文件时要按这个规则插进合适的位置，而不是简单追加到末尾。
const char* const kBaseSheets[] = {
    ":/resources/styles/base/widget.qss",    // QMainWindow/QGraphicsView
                                             // 等最宽的一层
    ":/resources/styles/base/button.qss",    // QAbstractButton 族
    ":/resources/styles/base/input.qss",     // QLineEdit 族、QAbstractSpinBox
                                             // 族、QComboBox
    ":/resources/styles/base/itemview.qss",  // QAbstractItemView
                                             // 族、QHeaderView
    ":/resources/styles/base/scrollbar.qss",
    ":/resources/styles/base/tab.qss",
    ":/resources/styles/base/container.qss",
    ":/resources/styles/base/menu.qss",
    ":/resources/styles/base/progress.qss",
    ":/resources/styles/base/label.qss",
};

QString ReadSheet(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    // 读不到就静默少一层样式，界面上看不出原因，所以这里要报出来
    qWarning("样式表读取失败，将无样式：%s", qPrintable(path));
    return QString();
  }
  return QString::fromUtf8(file.readAll());
}

}  // namespace

// 静态库里的 qrc 符号没人引用就不会被链进来，这里是那个引用。
//
// 单独放一个全局函数、不放进匿名命名空间也不是类成员：Q_INIT_RESOURCE 展开
// 后会声明并调用 qInitResources_style_res()，那个函数由 rcc 生成在全局作用域
// ——放进命名空间会去找一个不存在的 ns::qInitResources_style_res
static void InitStyleResources() { Q_INIT_RESOURCE(style_res); }

void ApplyStyleSheet(QApplication& app, const QStringList& project_sheets) {
  InitStyleResources();

  QString sheet;
  for (const char* path : kBaseSheets) {
    const QString text = ReadSheet(QString::fromLatin1(path));
    if (text.isEmpty()) {
      continue;
    }
    // 逐份之间补换行：文件末尾没有换行时会出现 `}QPushButton{` 粘连
    if (!sheet.isEmpty()) {
      sheet += QLatin1Char('\n');
    }
    sheet += text;
  }
  for (const QString& path : project_sheets) {
    const QString text = ReadSheet(path);
    if (text.isEmpty()) {
      continue;
    }
    if (!sheet.isEmpty()) {
      sheet += QLatin1Char('\n');
    }
    sheet += text;
  }
  app.setStyleSheet(sheet);

  // 全局字体走应用默认字体，不在 QSS 里用 `*` 铺 font-family：那样每个控件都要
  // 多做一次样式表匹配，而且会把控件已有的字号/字重一起冲掉
  QFont app_font = app.font();
  app_font.setFamily(QStringLiteral("Microsoft YaHei"));
  app.setFont(app_font);
}
