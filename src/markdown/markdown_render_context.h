#ifndef FILE_MARKDOWN_MARKDOWN_RENDER_CONTEXT_H_
#define FILE_MARKDOWN_MARKDOWN_RENDER_CONTEXT_H_

#include <memory>

#include "markdown_types.h"

// 每个 MarkdownView 一份的渲染状态：内存缓存、计数、配置。
// 引擎（QuickJS runtime + 载荷）是进程级共享的，不在这里；上下文本体
// 限定单线程使用（当前 GUI 线程），不可拷贝。
class MarkdownRenderContext {
 public:
  MarkdownRenderContext();
  ~MarkdownRenderContext();
  MarkdownRenderContext(const MarkdownRenderContext&) = delete;
  MarkdownRenderContext& operator=(const MarkdownRenderContext&) = delete;

  void SetOptions(const MarkdownRenderContextOptions& options);
  const MarkdownRenderContextOptions& options() const;
  void SetDiagnostics(const MarkdownDiagnostics& diagnostics);
  MarkdownDiagnostics diagnostics() const;
  // 本上下文计数 + 共享引擎状态；不触发任何加载
  MarkdownRendererStatus status() const;
  // 清公式/Mermaid 内存缓存；不动持久缓存。换文档时用
  void ClearMemoryCaches();
  // 清计数；loaded/permanently_failed 等引擎状态不动
  void ResetStats();

 private:
  friend struct MarkdownRenderContextAccess;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#endif  // FILE_MARKDOWN_MARKDOWN_RENDER_CONTEXT_H_
