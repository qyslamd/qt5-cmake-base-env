#ifndef FILE_STYLE_STYLE_H_
#define FILE_STYLE_STYLE_H_

// 底座层样式表与箭头图**拷贝自 qt-libui 仓库**（对方已独立发展，两边不再同步；
// 本仓只需要这份稳定的 QSS 底座）。上游若有修复要带过来，得手工比一遍——
// 见 docs/plan/plan总结.md 的「界面双路线与 FFT 选型」D4。
//
// 与项目层的分工：底座只写类型/伪状态/子控件/属性选择器，项目层（本仓
// src/app/resources/styles/default.qss）只写挂在 objectName 上的规则

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
