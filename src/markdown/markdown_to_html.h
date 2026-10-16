#ifndef FILE_MARKDOWN_MARKDOWN_TO_HTML_H_
#define FILE_MARKDOWN_MARKDOWN_TO_HTML_H_

#include <functional>

#include <QString>

#include "heading_ids.h"

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

// 代码块回调：每个代码块渲染完都会调一次（id，原始源码）。宿主拿它给
// HTML 里 `<a href="copy:ID">复制</a>` 备好原文——渲染层不碰剪贴板。
// 传进来的意义同样是依赖倒置：不传就没有复制按钮可查的表，单测塞个 lambda
// 就能验 id 与源码的对应
using CodeBlockSink =
    std::function<void(const QString& id, const QString& code)>;

// Markdown → HTML 正文（不含 <html>/<style> 外壳，由调用方套样式）。
// 按 GFM 方言解析：表格、删除线、任务列表都在这个 dialect 里，用默认 flags
// 会把表格静默丢掉。围栏代码块交给 HighlightCode 上色、包一层带"复制"链接
// 的框；$…$/$$…$$ 交给 formula 渲染（不传则原样保留）；```mermaid 交给
// mermaid 渲染（不传则退回代码块）。标题一律补 id 与 data-md-slug，并把
// 「slug → 元素 id」交给 heading_ids——不给回调时锚点表就没建，页内 #跳转
// 会原地不动。code_blocks 同理：不传则复制链接点了没反应（见 MarkdownView）。
QString MarkdownToHtml(const QString& markdown, const FormulaSink& formula = {},
                       const MermaidSink& mermaid = {},
                       const CodeBlockSink& code_blocks = {},
                       const HeadingSink& heading_ids = {});

#endif  // FILE_MARKDOWN_MARKDOWN_TO_HTML_H_
