#ifndef FILE_MARKDOWN_MARKDOWN_TO_HTML_H_
#define FILE_MARKDOWN_MARKDOWN_TO_HTML_H_

#include <QString>

// Markdown → HTML 正文（不含 <html>/<style> 外壳，由调用方套样式）。
// 按 GFM 方言解析：表格、删除线、任务列表都在这个 dialect 里，用默认 flags
// 会把表格静默丢掉。围栏代码块交给 HighlightCode 上色。
QString MarkdownToHtml(const QString& markdown);

#endif  // FILE_MARKDOWN_MARKDOWN_TO_HTML_H_
