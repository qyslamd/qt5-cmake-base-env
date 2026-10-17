#ifndef FILE_MARKDOWN_DEFAULT_RENDER_CONTEXT_H_
#define FILE_MARKDOWN_DEFAULT_RENDER_CONTEXT_H_

class MarkdownRenderContext;

// 便捷重载（两参 HighlightCode / FormulaToImgTag / MermaidToImgTag 与旧
// Clear*Cache）共用的进程默认上下文。持久缓存默认关闭：直接调用者不应
// 悄悄往用户缓存目录写文件，需要持久复用的宿主用 MarkdownView。
//
// **只能在 GUI 线程调用**：函数级静态只保证初始化安全，不保证后续访问；
// 渲染器本身还依赖 GUI 线程的字体度量与 QSvgRenderer
MarkdownRenderContext& DefaultRenderContext();

#endif  // FILE_MARKDOWN_DEFAULT_RENDER_CONTEXT_H_
