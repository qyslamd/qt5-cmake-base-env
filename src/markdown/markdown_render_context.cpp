#include "markdown_render_context.h"

#include "markdown_render_context_internal.h"
#include "renderer_status_internal.h"

struct MarkdownRenderContext::Impl {
  MarkdownRenderContextState state;
};

MarkdownRenderContext::MarkdownRenderContext() : impl_(new Impl) {}

MarkdownRenderContext::~MarkdownRenderContext() = default;

void MarkdownRenderContext::SetOptions(
    const MarkdownRenderContextOptions& options) {
  impl_->state.options = options;
  impl_->state.formula_cache.max_entries = options.formula_cache_entries;
  impl_->state.mermaid_cache.max_entries = options.mermaid_cache_entries;
  // 调小上限立即生效，不让旧条目继续占着
  impl_->state.formula_cache.Trim();
  impl_->state.mermaid_cache.Trim();
}

const MarkdownRenderContextOptions& MarkdownRenderContext::options() const {
  return impl_->state.options;
}

void MarkdownRenderContext::SetDiagnostics(
    const MarkdownDiagnostics& diagnostics) {
  impl_->state.options.diagnostics = diagnostics;
}

MarkdownDiagnostics MarkdownRenderContext::diagnostics() const {
  return impl_->state.options.diagnostics;
}

MarkdownRendererStatus MarkdownRenderContext::status() const {
  MarkdownRendererStatus status;
  status.highlighter = HighlightEngineStatus();
  status.formula = FormulaEngineStatus();
  status.mermaid = MermaidEngineStatus();
  status.highlight_context = impl_->state.highlight_stats;
  status.formula_context = impl_->state.formula_stats;
  status.mermaid_context = impl_->state.mermaid_stats;
  status.image_context = impl_->state.image_stats;
  return status;
}

void MarkdownRenderContext::ClearMemoryCaches() {
  impl_->state.formula_cache.Clear();
  impl_->state.mermaid_cache.Clear();
}

void MarkdownRenderContext::ResetStats() {
  impl_->state.highlight_stats = MarkdownContextStatus();
  impl_->state.formula_stats = MarkdownContextStatus();
  impl_->state.mermaid_stats = MarkdownContextStatus();
  impl_->state.image_stats = MarkdownContextStatus();
}

MarkdownRenderContextState& MarkdownRenderContextAccess::State(
    MarkdownRenderContext& context) {
  return context.impl_->state;
}
