#ifndef FILE_MARKDOWN_MARKDOWN_RENDER_CONTEXT_INTERNAL_H_
#define FILE_MARKDOWN_MARKDOWN_RENDER_CONTEXT_INTERNAL_H_

#include <limits>

#include <QCache>
#include <QHash>
#include <QList>
#include <QPixmap>
#include <QString>

#include "markdown_render_context.h"

// 渲染器内部的字符串缓存（公式 tag / Mermaid tag）。失败结果也缓存空串：
// 同一个坏输入不该每次编辑都重试。FIFO 淘汰，上限由上下文选项控制
struct RenderStringCache {
  QHash<QString, QString> entries;
  QList<QString> order;
  int max_entries = 256;

  const QString* Find(const QString& key) const {
    const auto it = entries.constFind(key);
    return it == entries.constEnd() ? nullptr : &it.value();
  }

  void Insert(const QString& key, const QString& value) {
    if (!entries.contains(key)) {
      order.append(key);
    }
    entries.insert(key, value);
    Trim();
  }

  // 上限调小后立即生效，不必等下一次 Insert
  void Trim() {
    while (order.size() > max_entries && !order.isEmpty()) {
      entries.remove(order.takeFirst());
    }
  }

  void Clear() {
    entries.clear();
    order.clear();
  }
};

// token → 已解码的 pixmap 与 token 化后的 img tag。宿主只按 token 查。
struct PixmapCacheEntry {
  QPixmap pixmap;
  QString img_tag;
};

// 渲染器能看到的上下文状态。引擎状态单独由各渲染器通过
// renderer_status_internal.h 的查询函数提供
struct MarkdownRenderContextState {
  // 诊断配置只存在 options 里，避免两份状态漂移
  MarkdownRenderContextOptions options;
  RenderStringCache formula_cache;
  RenderStringCache mermaid_cache;
  // 公式/图表图片，跨 ApplyDocument 复用；当前文档的 pin 在 MarkdownView
  QCache<QString, PixmapCacheEntry> pixmap_cache;
  MarkdownContextStatus highlight_stats;
  MarkdownContextStatus formula_stats;
  MarkdownContextStatus mermaid_stats;
  MarkdownContextStatus image_stats;

  // 缓存上限必须与对外 options 一致；RenderStringCache 自带的 256 只是
  // 兜底，不允许成为实际行为（否则 options() 与实际缓存分叉）
  MarkdownRenderContextState() {
    formula_cache.max_entries = options.formula_cache_entries;
    mermaid_cache.max_entries = options.mermaid_cache_entries;
    UpdatePixmapCacheLimit();
  }

  // QCache 的 cost 是 int；0/负值沿用模块惯例表示无上限
  void UpdatePixmapCacheLimit() {
    const qint64 bytes = options.image_cache_bytes;
    constexpr qint64 kMaxCost = std::numeric_limits<int>::max();
    pixmap_cache.setMaxCost(bytes > 0 ? static_cast<int>(qMin(bytes, kMaxCost))
                                      : INT_MAX);
  }
};

struct MarkdownRenderContextAccess {
  static MarkdownRenderContextState& State(MarkdownRenderContext& context);
};

#endif  // FILE_MARKDOWN_MARKDOWN_RENDER_CONTEXT_INTERNAL_H_
