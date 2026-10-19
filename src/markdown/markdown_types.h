#ifndef FILE_MARKDOWN_MARKDOWN_TYPES_H_
#define FILE_MARKDOWN_MARKDOWN_TYPES_H_

#include <functional>

#include <QString>

// 大纲条目：level 是 1..6，text 是去掉内联标签后的纯文本，anchor 是最终
// slug（重名标题带 -1/-2 后缀），可直接传给 MarkdownView::ScrollToAnchor()
struct MarkdownHeading {
  int level = 0;
  QString text;
  QString anchor;
};

// 会改变 HTML 结构的渲染选项。渲染开关（公式/Mermaid）不在这里：纯函数
// 通过"传不传 sink"表达，视图层再翻译成这几个字段
struct MarkdownRenderOptions {
  bool code_line_numbers = false;
  std::function<void(const MarkdownHeading&)> outline;
  std::function<QString(const QString& code, const QString& language)>
      highlight;
};

enum class MarkdownTheme {
  Light,
  Dark,
};

// 诊断开关：trace 打开后按阶段打日志；dump_directory 非空时落中间产物。
// 二者默认关闭，计数始终存在（几个整数自增）
struct MarkdownDiagnostics {
  bool trace = false;
  QString dump_directory;
};

// 引擎状态：进程级（每个渲染器一个引擎，跨视图共享）
struct MarkdownEngineStatus {
  bool loaded = false;
  bool permanently_failed = false;
  qint64 first_load_ms = 0;
};

// 上下文状态：视图级计数
struct MarkdownContextStatus {
  quint64 renders = 0;
  quint64 failures = 0;
  quint64 memory_cache_hits = 0;
  quint64 memory_cache_misses = 0;
  quint64 disk_cache_hits = 0;
  quint64 disk_cache_misses = 0;
  quint64 image_cache_hits = 0;
  quint64 image_cache_misses = 0;
};

struct MarkdownRendererStatus {
  MarkdownEngineStatus highlighter;
  MarkdownEngineStatus formula;
  MarkdownEngineStatus mermaid;
  MarkdownContextStatus highlight_context;
  MarkdownContextStatus formula_context;
  MarkdownContextStatus mermaid_context;
  MarkdownContextStatus image_context;
};

// 上下文配置：缓存目录、容量、诊断。默认值即 MarkdownView 的默认行为
struct MarkdownRenderContextOptions {
  bool persistent_cache_enabled = true;
  // 空 = QStandardPaths::CacheLocation + "/markdown-render"；
  // 非空 = 直接作为本模块的缓存根目录
  QString cache_directory;
  qint64 image_cache_bytes = 64LL * 1024 * 1024;
  // 条目上限同时约束内存与磁盘；字节上限只约束磁盘
  int formula_cache_entries = 512;
  int mermaid_cache_entries = 64;
  qint64 formula_cache_bytes = 16LL * 1024 * 1024;
  qint64 mermaid_cache_bytes = 128LL * 1024 * 1024;
  MarkdownDiagnostics diagnostics;
};

#endif  // FILE_MARKDOWN_MARKDOWN_TYPES_H_
