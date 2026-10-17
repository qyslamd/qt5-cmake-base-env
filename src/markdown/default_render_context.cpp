#include "default_render_context.h"

#include <memory>

#include "markdown_render_context.h"

MarkdownRenderContext& DefaultRenderContext() {
  static std::unique_ptr<MarkdownRenderContext> context = [] {
    auto created = std::make_unique<MarkdownRenderContext>();
    MarkdownRenderContextOptions options;
    options.persistent_cache_enabled = false;
    created->SetOptions(options);
    return created;
  }();
  return *context;
}
