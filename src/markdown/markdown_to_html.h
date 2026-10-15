#ifndef FILE_MARKDOWN_MARKDOWN_TO_HTML_H_
#define FILE_MARKDOWN_MARKDOWN_TO_HTML_H_

#include <functional>

#include <QString>

// 公式渲染回调：tex 原文 + 是否独立公式 → 可直接内联的 HTML（通常是 <img>）。
// 返回空串表示渲染不出来，正文里会保留原文（红色 <code class="math-src">）。
//
// 传进来的意义是**依赖倒置**：markdown 模块本身不依赖公式引擎（QuickJS 那套
// 在 formula_renderer 里），单元测试塞一个假实现就能验抽取与回填；不要公式的
// 宿主什么都不传，$…$ 原样显示。
using FormulaSink = std::function<QString(const QString& tex, bool display)>;

// mermaid 渲染回调：mermaid 源码（实体已还原）→ 可直接内联的 HTML。返回空串
// 表示渲染不出来，原位退回代码块高亮。与 FormulaSink 同一套理由：markdown
// 模块不依赖渲染引擎，单测塞假实现即可。
using MermaidSink = std::function<QString(const QString& code)>;

// Markdown → HTML 正文（不含 <html>/<style> 外壳，由调用方套样式）。
// 按 GFM 方言解析：表格、删除线、任务列表都在这个 dialect 里，用默认 flags
// 会把表格静默丢掉。围栏代码块交给 HighlightCode 上色；$…$/$$…$$ 交给
// formula 渲染（不传则原样保留）。
QString MarkdownToHtml(const QString& markdown, const FormulaSink& formula = {},
                       const MermaidSink& mermaid = {});

#endif  // FILE_MARKDOWN_MARKDOWN_TO_HTML_H_
