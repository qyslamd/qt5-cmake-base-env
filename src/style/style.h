#ifndef FILE_STYLE_STYLE_H_
#define FILE_STYLE_STYLE_H_

#include <QStringList>

class QApplication;

// 把样式表装到应用上：先是本模块自带的底座层（base/*.qss，只写类型、伪状态、
// 子控件、属性选择器，与业务无关），再是调用方给的**项目层**（本仓业务控件的
// objectName 规则，见 src/app/resources/styles/default.qss）。
//
// 底座层内部顺序固定为"从宽到窄"，见 style.cpp。项目层在后，且它的 id 选择器
// 具体度更高，两层重合时以项目层为准。
void ApplyStyleSheet(QApplication& app, const QStringList& project_sheets = {});

#endif  // FILE_STYLE_STYLE_H_
