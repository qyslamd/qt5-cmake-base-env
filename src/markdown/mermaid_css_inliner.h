#ifndef FILE_MARKDOWN_MERMAID_CSS_INLINER_H_
#define FILE_MARKDOWN_MERMAID_CSS_INLINER_H_

#include <QString>

// 把 SVG 里 <style> 块的 CSS 规则内联到各元素的属性上。
//
// QSvgRenderer 不认 <style> 块（mermaid 的样式 99% 在里面），而且它处理 style
// 与 presentation attribute 的优先级跟 CSS 规范相反——它认 attribute，所以命中
// 的声明要同时写进属性。
//
// font_family 非空时，所有元素的 font-family 都被强制成它：mermaid 写的是
// "trebuchet ms",verdana,...，QSvgRenderer 会去系统里找替代字体，与度量用的
// 那份对不上（量画不同源）。传空串表示不改写。
QString InlineSvgCss(const QString& svg, const QString& font_family);

#endif  // FILE_MARKDOWN_MERMAID_CSS_INLINER_H_
