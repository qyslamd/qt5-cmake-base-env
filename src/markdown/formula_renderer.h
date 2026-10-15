#ifndef FILE_MARKDOWN_FORMULA_RENDERER_H_
#define FILE_MARKDOWN_FORMULA_RENDERER_H_

#include <QString>

// TeX 公式 → 可直接内联进 HTML 的 <img>（src 是 PNG 的 data URI）。
//
// 为什么是图：qlitehtml 排不动 KaTeX/MathJax 的 vlist 布局（inline-table 不
// 收缩到内容宽、负偏移塌陷——实测分式分数线拉成通栏），但画图没问题。所以
// 排版交给 QuickJS 里的 MathJax（tex→svg），Qt 再把 SVG 画成位图。
//
// 首次调用才加载 JS 载荷（1.5MB 且首次求值要一两秒），没公式的文档不付这个
// 钱；结果按 (tex, display) 缓存。渲染失败返回空串——调用方按"显示原文"兜底。
QString FormulaToImgTag(const QString& tex, bool display);

// JS 载荷是否可用（加载失败即永久不可用，不会反复重试）
bool FormulaRendererReady();

#endif  // FILE_MARKDOWN_FORMULA_RENDERER_H_
