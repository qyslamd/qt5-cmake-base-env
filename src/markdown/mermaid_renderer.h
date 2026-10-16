#ifndef FILE_MARKDOWN_MERMAID_RENDERER_H_
#define FILE_MARKDOWN_MERMAID_RENDERER_H_

#include <QString>

// mermaid 源码 → 可直接内联进 HTML 的 <img>（src 是 PNG 的 data URI）。
//
// 为什么不引 Node/WebEngine：与公式那条路同一套约束——QuickJS 里跑官方
// mermaid.js 出 SVG，Qt 再把 SVG 画成位图。mermaid 与 MathJax 的差别在于它
// 要问宿主"这段字多宽"才决定连线怎么走，所以多一份假 DOM（dom_shim.js）和
// 一组度量回调（用 QFontMetricsF 回答，与绘制共用同一份字体）。
//
// 三份载荷放在 qrc 里（3.5MB），首次调用才求值；结果按源码缓存，**失败也缓存
// 空串**（同一个坏语法不该每次编辑都重试）。渲染失败返回空串。
//
// qrc 载荷的初始化由本模块内部完成（Q_INIT_RESOURCE
// 幂等），宿主不需要做任何事。
QString MermaidToImgTag(const QString& code);

// 载荷是否可用（加载失败即永久不可用，不会反复重试）
bool MermaidRendererReady();

// 度量与绘制共用的字体族名。空串表示没找到带 CJK 的族，此时不改写 SVG 里的
// font-family（度量按 SVG 自己写的族名走）。供排查与单测用。
//
// **前置条件：GUI 线程、QGuiApplication 已构造之后**——内部要查 QFontDatabase，
// 度量也要建 QFontMetricsF，两者都依赖 GUI 应用实例。将来若把渲染搬进后台线程，
// 线程里得先有 QGuiApplication（或把族名在 GUI 线程算好传进去）。
QString MermaidFontFamily();

#endif  // FILE_MARKDOWN_MERMAID_RENDERER_H_
