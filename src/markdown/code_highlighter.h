#ifndef FILE_MARKDOWN_CODE_HIGHLIGHTER_H_
#define FILE_MARKDOWN_CODE_HIGHLIGHTER_H_

#include <QString>

// 源码文本 → 带 <span class="kw|str|num|cmt"> 的 HTML 片段。
// 这四个类名是与样式表（markdown_default.css）的唯一契约，改配色动 CSS 即可。
// language 认不出时按 C++ 规则处理；输出已做 HTML 转义，可直接进 HTML。
QString HighlightCode(const QString& code, const QString& language);

#endif  // FILE_MARKDOWN_CODE_HIGHLIGHTER_H_
