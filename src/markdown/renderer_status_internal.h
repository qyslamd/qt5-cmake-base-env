#ifndef FILE_MARKDOWN_RENDERER_STATUS_INTERNAL_H_
#define FILE_MARKDOWN_RENDERER_STATUS_INTERNAL_H_

#include "markdown_types.h"

// 各渲染器的引擎状态（进程级）。查询不触发加载：只读已加载标志与耗时
MarkdownEngineStatus HighlightEngineStatus();
MarkdownEngineStatus FormulaEngineStatus();
MarkdownEngineStatus MermaidEngineStatus();

#endif  // FILE_MARKDOWN_RENDERER_STATUS_INTERNAL_H_
