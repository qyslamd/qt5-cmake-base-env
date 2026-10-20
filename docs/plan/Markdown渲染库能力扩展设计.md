# Markdown 渲染库能力扩展设计

状态：**定稿**（2026-10-17）：三轮审查无 🔴；用户评审意见（§十一）已逐条核实并处置。
实施状态（2026-10-20）：主体能力已落地；图片字节 LRU（阶段 1）与懒加载
（阶段 2，补丁 0007）暂缓，见 `Markdown图片懒加载设计.md` 与 `todo.md`。

## 一、需求汇总与目标

把 `src/markdown` 从"能渲染"推进到"可配置、可观测、可复用"的基础库：

| # | 能力 | 开关/默认 | 目标 |
|---|---|---|---|
| 1 | 代码块行号 | 默认关 | 表格行号；关闭时 HTML 与现状逐字节一致 |
| 2 | 公式渲染 | 默认开 | 关闭时不传 FormulaSink，`$…$` 原样显示，不触发 JS |
| 3 | Mermaid 渲染 | 默认开 | 关闭时不传 MermaidSink，围栏退回代码块 |
| 4 | 渲染结果持久缓存 | 默认开 | 公式/Mermaid 的 `<img>` 按内容哈希落盘，跨文档、跨进程复用 |
| 5 | 渲染诊断状态 | 始终可读 | 就绪、首次加载耗时、渲染/失败/缓存命中计数 |
| 6 | 渲染跟踪与转储 | 默认关 | 开关打开后按阶段打日志、落中间产物 |
| 7 | 图片懒加载 | 默认关 | 阶段 1：图片字节缓存；阶段 2（补丁）另评估 |
| 8 | 大纲侧栏 | 独立控件 | 数据 + `MarkdownOutlineWidget`，宿主决定放置 |
| 9 | 主题切换 | 默认亮色 | 亮/暗；CSS、公式前景色、Mermaid 主题一起切 |

### 非目标

- 不做"每个代码块单独开关"；
- 本轮不把渲染搬到后台线程（为后续迁移留接口，见 §三.4）；
- 不为诊断引入对 `src/logging` 的依赖（库保持独立，用 Qt 日志类别）。

## 二、设计约束

1. **不破坏原功能**：所有开关默认值保持现状；关闭路径直接走现有分支，
   不经过新代码；`MarkdownToHtml` 默认输出与现有测试逐字节不变。
2. **可调试**：新能力都配状态计数、跟踪日志、产物转储；缓存可查、可清、
   可关；失败不静默（沿用 qWarning，不吞异常）。
3. **SOLID**：
   - SRP：`render_cache` / `render_diagnostics` / `renderer_status` /
     `MarkdownOutlineWidget` 各自独立，`MarkdownView` 只做门面与编排；
   - OCP：选项走 `MarkdownRenderOptions`，主题走枚举，加能力不改签名；
   - DIP：`markdown_to_html` 只依赖 sink 抽象；渲染器参数显式传入，
     不反向依赖视图。
4. **为异步渲染留路**（当前 JS 引擎阻塞 UI 线程，后续再优化）：
   - 渲染函数不碰 QWidget；颜色/主题/字体族显式传参；
   - **引擎（QuickJS runtime + 载荷）进程级共享**，**状态（缓存/计数/配置）
     按视图隔离**（`MarkdownRenderContext`）；
   - 共享引擎的加载状态用 `std::once_flag` / 原子量保护；上下文本身
     限定单线程使用（当前 GUI 线程），将来可整体搬到工作线程。

## 三、现状与边界（实据）

### 3.1 已经做到的部分

- 公式与 Mermaid 本来就是懒加载：首次调用才 `EnsureLoaded()`，
  `MarkdownView` 构造不预热（`markdown_view.cpp:159-163`、
  `formula_renderer.cpp:165`、`mermaid_renderer.cpp:556`）。
- 结果按内容缓存在内存：公式 `formula_renderer.cpp:23-40`，Mermaid
  `mermaid_renderer.cpp:40-53`；换文档时 `MarkdownView::LoadFile` 清内存缓存。
- `FormulaRendererReady()` / `MermaidRendererReady()` **会触发加载**，
  不能直接做状态查询。
- 代码高亮类名与 CSS 已解耦（`highlight_entry.js` 的 CLASS_MAP）。

### 3.2 外部约束

| 约束 | 位置 | 影响 |
|---|---|---|
| 资源回调同步 `QByteArray(QUrl)` | `qlitehtmlwidget.h:47` | 图片懒加载不能只靠回调 |
| `load_image` 只在 `compute_styles()`（建文档）调用 | `el_image.cpp:60-73`、`document.cpp:131` | 删 `m_pixmaps` 再重排不会重新加载，会得到 0×0 |
| `get_image_size` 读 `m_pixmaps` | `container_qpainter.cpp:771-784` | 占位图必须按真实固有尺寸生成 |
| `setDocument` 每次清空 `m_pixmaps` | `container_qpainter.cpp:1159-1167` | 每次重渲染都重读/重解码所有图片 |
| `redraw_on_ready` 被忽略；无图片失效 API | `container_qpainter.cpp:633` | 需要补丁 |
| `anchorY` 只在 DocumentContainer 公开 | `container_qpainter.h:58` | 大纲联动需要补丁暴露 |
| 无 `user-select` | qlitehtml 能力 | 行号会进 Ctrl+C 选区（接受） |
| litehtml 0.10 无 `word-wrap` 实现 | litehtml 源码 | 超长 token 不折行，与现状一致 |
| `text-indent` 支持 | `css_properties.cpp:227`、`render_inline_context.cpp:149-151,206-208` | 悬挂缩进方案可行 |
| `white-space: pre-wrap` 支持 | `el_space.cpp:11-27`、`line_box.cpp:733` | 代码块折行与续行对齐可行 |

## 四、总体架构

### 4.1 引擎与上下文分离

```
进程级（每渲染器一个引擎，跨视图共享，懒加载）
  ├─ highlighter：QuickJS runtime + highlight.js 载荷
  ├─ formula：QuickJS runtime + MathJax 载荷
  ├─ mermaid：QuickJS runtime + 三份载荷
  └─ 引擎状态：loaded / permanently_failed / first_load_ms

MarkdownView 实例级
  └─ MarkdownRenderContext（std::unique_ptr，pimpl）
       ├─ 内存缓存：公式、Mermaid、图片字节 LRU
       ├─ 计数：renders / failures / 命中
       ├─ 配置：主题、持久缓存目录/开关、诊断 trace/dump、图片懒加载
       └─ 渲染器函数通过显式参数拿到它
```

- **三个渲染器各自保留一个 runtime**，不合并：各自的桥接回调、全局变量
  （`__code`、`__renderId`、dom_shim 的 document/timer）互不相同，合并
  风险大于收益；"共享"指跨视图共享同一份引擎，而不是三个引擎合一；
- `MarkdownRenderContext` 不可拷贝，限定单线程使用；它只持有缓存/计数/
  配置，不持有 runtime；
- 渲染器提供 `ctx` 版本与"进程默认 ctx"便捷重载，现有直接调用点可继续
  用两参/一参形式；
- 同一进程多个视图各有缓存与计数，主题互不污染；Mermaid 的
  `initialize` 仍是引擎级状态，主题变化时按需重设（见 §6.7）；
- **异步演进边界**：本轮不搬线程。`MarkdownRenderContext` 的状态是
  线程限定的，将来可以整体搬到工作线程；但 Mermaid 的度量链路
  （`MermaidFontFamily()` / `MetricsFor` / `QFontMetricsF`）依赖
  GUI 线程与 `QGuiApplication`，届时必须在 GUI 线程算好字体族/度量再
  传入，文档不承诺"一行不改就能异步"。`QSvgRenderer` 的线程安全性也需
  单独验证。

### 4.2 `MarkdownToHtml` 增加高亮 sink 与选项

目前 `markdown_to_html.cpp` 直接调用 `HighlightCode`。为了按视图隔离高亮
状态，把高亮也做成 sink（默认回落到现有全局实现）：

```cpp
struct MarkdownRenderOptions {
  bool code_line_numbers = false;
  std::function<void(const MarkdownHeading&)> outline;
  std::function<QString(const QString& code, const QString& language)> highlight;
};

// options 固定为第 6 参：现有调用点（含测试）把 heading_ids 放在第 5 参，
// 不能把 options 插到它前面
QString MarkdownToHtml(const QString& markdown, const FormulaSink& formula = {},
                       const MermaidSink& mermaid = {},
                       const CodeBlockSink& code_blocks = {},
                       const HeadingSink& heading_ids = {},
                       const MarkdownRenderOptions& options = {});
```

`highlight` 为空 → 走现在的 `HighlightCode`，默认行为不变。

大纲需要 level/纯文本/slug，而现有 `HeadingSink` 只有 slug/id。给
`AddHeadingIds` 加一个可选的大纲回调——**直接复用 `MarkdownHeading` 类型**，
不另造一个函数签名，`MarkdownToHtml` 因此不需要适配层：

```cpp
QString AddHeadingIds(
    const QString& html, const HeadingSink& on_heading = {},
    const std::function<void(const MarkdownHeading&)>& on_outline = {});
```

`MarkdownToHtml` 把 `options.outline` 原样转发给 `on_outline`。

## 五、公共 API

### 5.1 公共类型（`markdown_types.h`）

```cpp
struct MarkdownHeading {
  int level = 0;          // 1..6
  QString text;           // 纯文本（不含内联标签）
  QString anchor;         // slug，可直接传给 ScrollToAnchor()
};

enum class MarkdownTheme { Light, Dark };

struct MarkdownDiagnostics {
  bool trace = false;         // 按阶段 qCDebug + 计时
  QString dump_directory;     // 空 = 不落产物
};
```

### 5.2 渲染上下文与渲染器

```cpp
struct MarkdownRenderContextOptions {
  bool persistent_cache_enabled = true;  // 便捷默认 ctx 里为 false（见下）
  // 空 = QStandardPaths::CacheLocation + "/markdown-render"；
  // 非空 = 直接作为本模块的缓存根目录（不再拼子目录）
  QString cache_directory;
  qint64 image_cache_bytes = 64 * 1024 * 1024;
  // 条目上限同时约束内存与磁盘；字节上限只约束磁盘
  int formula_cache_entries = 512;
  int mermaid_cache_entries = 64;
  qint64 formula_cache_bytes = 16 * 1024 * 1024;
  qint64 mermaid_cache_bytes = 128 * 1024 * 1024;
  MarkdownDiagnostics diagnostics;
};

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
  MarkdownRendererStatus status() const;  // 本上下文计数 + 共享引擎状态
  void ClearMemoryCaches();  // LoadFile 换文件时用；不动持久缓存
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// ctx 版本（MarkdownView 使用）
QString HighlightCode(MarkdownRenderContext& ctx, const QString& code,
                      const QString& language);
QString FormulaToImgTag(MarkdownRenderContext& ctx, const QString& tex,
                        bool display, const QColor& color,
                        QString* cache_key = nullptr);
QString MermaidToImgTag(MarkdownRenderContext& ctx, const QString& code,
                        MarkdownTheme theme, QString* cache_key = nullptr);

// 便捷重载（进程默认 ctx，供测试/直接调用；保持现有调用点可编译）
QString HighlightCode(const QString& code, const QString& language);
QString FormulaToImgTag(const QString& tex, bool display);
QString MermaidToImgTag(const QString& code);
```

- 便捷重载内部使用一个函数级静态默认 `MarkdownRenderContext`；
- **便捷重载只能在 GUI 线程调用**：默认上下文不跨线程（函数级静态只保证
  初始化安全，不保证后续访问），渲染器本身还依赖 GUI 线程的字体度量与
  `QSvgRenderer`；与 `MarkdownView` 的线程约束一致；
- **便捷默认 ctx 的 `persistent_cache_enabled = false`**：直接调用者与
  现有测试不会往真实的 `QStandardPaths::CacheLocation` 写文件；只有
  `MarkdownView` 持有的上下文默认开启；
- 便捷 `FormulaToImgTag(tex, display)` 保持旧行为：不替换颜色、缓存 key
  与现在一致，现有 PNG 不变；
- 便捷 `MermaidToImgTag(code)` **固定按 Light 主题渲染**（需要时重新
  initialize），不继承上一个 ctx 调用留下的主题状态，现有测试结果稳定；
- 旧公开 API 保留并明确语义：`FormulaRendererReady()` /
  `MermaidRendererReady()` / `ClearFormulaCache()` / `ClearMermaidCache()`
  都作用于**默认 ctx**；`MarkdownView` 另外提供 `ClearMemoryCaches()`
  清本视图缓存，宿主按旧注释调全局 Clear 不会清到视图缓存——头文件注释
  同步改；
- 两个参数个数不同的重载都能被两参/一参 sink 接受（不会出现"默认参数
  导致函数类型变成三参"的问题）。

### 5.3 `MarkdownView`

```cpp
void SetFormulaRenderingEnabled(bool enabled);   // 默认 true
bool formula_rendering_enabled() const;
void SetMermaidRenderingEnabled(bool enabled);   // 默认 true
bool mermaid_rendering_enabled() const;

void SetCodeLineNumbers(bool enabled);           // 默认 false
bool code_line_numbers() const;

void SetTheme(MarkdownTheme theme);              // 默认 Light
MarkdownTheme theme() const;

void SetPersistentRenderCacheEnabled(bool enabled);  // 默认 true
bool persistent_render_cache_enabled() const;
void SetRenderCacheDirectory(const QString& dir);
void ClearPersistentRenderCache();

void SetDiagnostics(const MarkdownDiagnostics& diagnostics);
MarkdownDiagnostics diagnostics() const;

void SetImageLazyLoadingEnabled(bool enabled);   // 默认 false（阶段 1）
bool image_lazy_loading_enabled() const;

const QVector<MarkdownHeading>& outline() const;
// 找不到锚点返回 nullopt；0 是合法文档坐标（标题在文档最顶部）
std::optional<int> AnchorY(const QString& anchor) const;

MarkdownRendererStatus renderer_status() const;  // 本次视图的上下文
void ResetRendererStats();                       // 只清计数，不清 loaded

// 本视图的上下文（改缓存目录/容量、读计数）；生命周期跟随 MarkdownView
MarkdownRenderContext& render_context();
const MarkdownRenderContext& render_context() const;
// 清本视图的公式/Mermaid 内存缓存。全局 ClearFormulaCache() /
// ClearMermaidCache() 只作用于进程默认上下文，对视图无效
void ClearRenderCaches();

signals:
  void outlineChanged();
  void viewportScrolled(int y);                  // 文档坐标
```

`Set*` 的重渲染契约按影响面区分：

- 影响 HTML 的（主题、行号、公式/Mermaid 开关）：值未变直接返回；变了且
  已有文档时重渲染并保留垂直滚动位置（选区不保证保留，文档注明）；
- 只影响 I/O 与观测的（持久缓存开关/目录、诊断 trace/dump、容量）：
  只更新上下文配置，不重渲染。

`LoadFile` 换文件时调用的是 **`ctx_.ClearMemoryCaches()`**，不再调用全局
`ClearFormulaCache()` / `ClearMermaidCache()`；`ApplyDocument` 负责把
高亮 sink、公式/Mermaid sink（捕获本视图主题）、大纲回调、行号选项、
`Diagnostics` 配置接到 `MarkdownToHtml`。`MarkdownView` 的改造量按此
清单估算，不是只加几个 setter。

### 5.4 诊断状态

```cpp
struct MarkdownEngineStatus {          // 进程级
  bool loaded = false;
  bool permanently_failed = false;
  qint64 first_load_ms = 0;
};

struct MarkdownContextStatus {         // 视图级
  quint64 renders = 0;
  quint64 failures = 0;
  quint64 memory_cache_hits = 0;
  quint64 memory_cache_misses = 0;
  quint64 disk_cache_hits = 0;
  quint64 disk_cache_misses = 0;
  quint64 image_cache_hits = 0;        // 图片字节 LRU
  quint64 image_cache_misses = 0;
};

struct MarkdownRendererStatus {
  MarkdownEngineStatus highlighter;
  MarkdownEngineStatus formula;
  MarkdownEngineStatus mermaid;
  MarkdownContextStatus highlight_context;
  MarkdownContextStatus formula_context;
  MarkdownContextStatus mermaid_context;
  MarkdownContextStatus image_context;  // 图片字节缓存命中
};
```

- `MarkdownView::renderer_status()` 与 `MarkdownRenderContext::status()`
  把共享引擎状态与本上下文计数拼起来；
- 查询**不触发加载**（只读标志位）；
- `ResetRendererStats()` 只清 `*_context`，不动引擎状态；
- 测试 8/9 的"未加载"断言走 `MarkdownView::renderer_status()`，并放在
  独立测试可执行文件里（引擎是进程级）。

## 六、功能设计

### 6.1 代码块行号

**HTML（开关打开）**

```html
<div class="code-wrap">
  <div class="code-header">…</div>
  <table class="md-code"><tbody>
    <tr><td class="md-code-ln">1</td><td class="md-code-line">…</td></tr>
  </tbody></table>
</div>
```

- 行号 1 起始、右对齐；空行单元格写 `&#160;`；
- 判据：`code.isEmpty()` → **零行**；否则去掉一个结尾 `'\n'` 再
  `split('\n')`（`"\n"` 得到一个空行，输出一行）；
- 关闭时保持现有 `<pre><code>` 结构，逐字节不变。

**行切分**：高亮载荷已在 JS 侧逐行闭合/重开 span
（`highlight_entry.js:119-146`；`code_highlighter_test.cpp:58` 钉住），
C++ 侧直接 `split('\n')`，注释写明契约。

**范围**：md4c 对围栏与缩进代码块都输出 `<pre><code>`
（`md4c-html.c:301`），无法区分，因此所有代码块（含无语言围栏与 Mermaid
渲染失败后的回落）都带行号。

**实现落点**：`BuildCodeBlockFrame(highlighted, language, block_id)` 增加
`bool line_numbers`；关闭分支保持现有 `<pre><code>` 代码一字不改，打开
分支生成表格。选项由 `MarkdownRenderOptions::code_line_numbers` 经
`HighlightCodeBlocks` 传入。

**CSS**（每套主题各一份）：

```css
.md-code { width: 100%; padding: 10px 0; background: #f6f8fa;
           border-collapse: collapse; }
.md-code td { border: none; padding: 0; vertical-align: top;
              font-family: Consolas, "Courier New", monospace;
              font-size: 13px; line-height: 20px; color: #24292e; }
.md-code .md-code-ln { width: 1%; padding: 0 10px 0 12px;
                       text-align: right; color: #8c959f;
                       white-space: nowrap; }
.md-code .md-code-line { padding: 0 12px 0 4px; padding-left: 2em;
                         text-indent: -2em; white-space: pre-wrap;
                         word-wrap: break-word; }
```

悬挂缩进按 demo 口径：折行续行右移 2em，明确"这是续行"。
`word-wrap` 在 litehtml 0.10 里没有实现（§3.2），保留该声明只是与现有
`pre code` 样式保持一致、等上游支持；不指望它今天生效。
`text-indent` 与 `white-space: pre-wrap` 都有实现（§3.2），方案可行。

**布局成本（实测）**：x86 relwithdebinfo、QLiteHtmlWidget 900×700、真实
CSS，`setHtml` 跑两次取耗时：

| 行数 | `<pre>` | `<table>` |
|---|---|---|
| 100 | 23 / 26 ms | 39 / 42 ms |
| 500 | 183 / 191 ms | 172 / 228 ms |
| 1000 | 737 / 738 ms | 363 / 375 ms |

表格不慢于 `<pre>`；千行文档的布局成本本身在数百毫秒级，行号不是主因。
样张 `temp/md_code_table_sample.png` 确认：行号右对齐、代码列对齐、
折行续行右移 2em、`width:1%` 的列宽稳定。

### 6.2 公式 / Mermaid 开关与懒加载

- 关闭公式：`ApplyDocument` 不传 `FormulaSink`，
  `MarkdownToHtml` 里 `formula ? ExtractMath(...) : markdown`，
  `$…$` 原样保留，不触发 FormulaToImgTag / MathJax 加载；
- 关闭 Mermaid：不传 `MermaidSink`，mermaid 围栏走普通代码块；
- 懒加载已经是现状，本次把它变成"可关"；开关变化重渲染并保留滚动。

### 6.3 持久化渲染缓存

内部模块 `render_cache.h/.cpp`，配置来自 `MarkdownRenderContext`：

```cpp
QString MakeRenderCacheKey(const QString& kind, const QString& payload,
                           const QString& variant);
bool LoadRenderCacheEntry(const RenderCacheConfig& config,
                          const QString& kind, const QString& key,
                          QString* tag);
void StoreRenderCacheEntry(const RenderCacheConfig& config,
                           const QString& kind, const QString& key,
                           const QString& tag);
```

- `MakeRenderCacheKey` 内部先调 **`EnsureMarkdownResources()`**（幂等）：
  静态库的 qrc 不会自动初始化，直接 `QFile::open(":/markdown/...")` 会
  失败，hash 为空会让持久缓存永远 miss；
- 目录：`QStandardPaths::CacheLocation + "/markdown-render/"`，
  每视图可覆盖（测试用临时目录）；
- 文件：`<kind>/<sha256>.txt`，内容为最终 `<img>` 标签，
  用 `QSaveFile` 原子替换；并发与损坏都按 miss/日志处理——
  `QSaveFile::commit()` 失败（另一进程/杀软占用）只记日志并继续；
  读到空文件、非 `<img` 开头、截断内容一律按 miss，删掉重写；
- key 组成（全部自动，不手工维护版本线）：
  - qrc 载荷哈希（公式 MathJax；Mermaid 三份）：**只读 qrc 字节直接
    做 SHA-256，不 eval**；哈希按进程缓存一次。这样新进程第一次查盘前
    就能算出 key，跨进程复用才成立（若先 `EnsureLoaded` 才哈希，等于
    先把 1.5MB/3.5MB JS 求值一遍）；
  - `QT_VERSION_STR`；
  - 公式：颜色 + display + tex；
  - Mermaid：主题标识（含 themeVariables 哈希）+ 字体指纹 + 源码；
    字体指纹 = `MermaidFontFamily()` + 该字体在 16px 下对 `"中A"` 的
    `QFontMetricsF`（ascent/descent/advance 量化）——字体文件更新或
    fallback 变化都会改变指纹，不依赖拿得到字体文件路径；
  - 另加一个手工 `kRenderPipelineVersion`，只覆盖 C++ 后处理逻辑变化；
- 命中/未命中：**不做"每次命中原位 setFileTime"**（GUI 线程上每个图一次
  系统调用不值）。淘汰用 **FIFO**：按文件创建/修改时间，写入时若
  "距上次清理已写入 N 条或目录超上限 10%"才批量扫描并删到 80%；
- **失败不落盘**；内存缓存仍缓存空串以避免重试；
- 容量：公式 512 条 / 16 MB，Mermaid 64 条 / 128 MB；上限在
  `MarkdownRenderContextOptions` 里，测试可注入小值；
- 开关关闭：跳过所有磁盘读写，内存缓存照常；
- 默认开启；`ClearPersistentRenderCache()` 只删 `markdown-render/` 子目录；
- 隐私：缓存里是文档渲染结果；默认开启、可关、只写模块子目录，文档说明。

### 6.4 渲染诊断：状态、跟踪、转储

**状态**见 §5.4；计数点：

| 点 | 计什么 |
|---|---|
| `HighlightCode(ctx, …)` | renders；failures 只计 JS 异常（未注册语言是正常回退，不计） |
| `FormulaToImgTag(ctx, …)` | renders / failures / 内存命中 / 磁盘命中 |
| `MermaidToImgTag(ctx, …)` | 同上 + 泵循环耗时 |
| 引擎加载 | first_load_ms / loaded / permanently_failed |

**跟踪**（`render_diagnostics.h/.cpp`，按上下文配置）：

```cpp
Q_DECLARE_LOGGING_CATEGORY(lcMarkdownRender)  // qt.markdown.render
```

- 类别 `qt.markdown.render` 在首次有人开启 trace 时
  `setEnabled(QtDebugMsg, true)`，**之后不再关闭**：类别是进程级的，
  两个视图一个开一个关时，关闭类别会让另一个也断掉。是否真的输出由
  **上下文里的 `trace` 标志**决定——每个上下文在打日志前先查自己的标志，
  所以多视图互不覆盖；
- 不依赖 `QT_LOGGING_RULES`；
- 阶段日志：ApplyDocument（文档大小/标题数/代码块数/公式数）、
  渲染器（命中/加载耗时/渲染耗时/失败原因）、Mermaid 泵（迭代/耗时/超预算）、
  资源回调（URL/字节数/拒绝原因）、持久缓存（读写/淘汰）。

**转储**（`dump_directory` 非空）：

```
<dump>/<yyyyMMdd-hhmmss>-<seq>/
  apply/<n>-source.md / <n>-final.html
  formula/<k>-<color>.svg / .png
  mermaid/<k>-raw.svg / clean.svg / inlined.svg / marked.svg / <k>.png
```

- 序号语义：`apply/` 按 `ApplyDocument` 次数递增（每次一源一最终），
  `formula/`、`mermaid/` 按渲染次数递增；各组独立编号、互不覆盖；
- 环境变量优先级：显式 `dump_directory` > `MD_MARKDOWN_DUMP` >
  `MD_MERMAID_DUMP`；
- `MD_MARKDOWN_DUMP` 覆盖全部产物（含文档正文）；
- `MD_MERMAID_DUMP` 是旧调试入口，只覆盖 `mermaid/` 前缀的中间产物，
  不会让文档正文落盘；
- 转储内容可能含文档正文，只在显式开启时写。

### 6.5 图片懒加载

**阶段 1（本轮，不补丁）**：上下文内图片字节 LRU。

- key：本地文件 = canonical path + mtime + size；data URI = URL 字符串；
- 容量 64 MB（在 `MarkdownRenderContextOptions` 里，可注入）；
- 命中直接返回字节，省掉重复读盘与 base64 解码；
- 命中/未命中计入 `image_cache_hits/misses`，测试可断言；
- 不改变布局与解码成本。

**阶段 2（另评估，qlitehtml 补丁）**：

详细设计见 `docs/plan/Markdown图片懒加载设计.md`（讨论中）。与本节原方案
的差异：

- 一轮审查确认：`load_image` 只在建文档时调用，删 `m_pixmaps` 后重排
  会得到 0×0。补丁改为 `replaceImage(const QUrl&, const QByteArray&)`：
  用新 pixmap 替换缓存条目；尺寸不变时只重绘，尺寸变化才重排并保留滚动；
- 原"按真实固有尺寸生成占位图"因内存开销（大图一张数 MB）改为补丁提供
  **尺寸查询回调**：pixmap 无效时 `get_image_size` 向宿主询问固有尺寸，
  不分配大占位图；
- 策略：首批 N 张同步返回真实数据，其余读文件头取尺寸后返回空字节，
  按事件循环分片逐张 `replaceImage`；
- 严格"视口内优先"不可行（`load_image` 在布局期调用，此时没有元素坐标），
  按"首批 N 张 + 字节预算"近似；
- 打印路径强制全量加载，避免 PDF 图片空白；
- 默认关。

### 6.6 大纲

**数据**：

- `MarkdownToHtml` 在补 `h-N id` 的同时通过 `options.outline` 交出
  `{level, text, anchor}`；`AddHeadingIds` 已有 level/plain/slug，
  不改 `HeadingSink` 签名；
- `MarkdownView` 在 `ApplyDocument` 收集，提供 `outline()` 与
  `outlineChanged()`；
- 滚动联动：`viewportScrolled(int y)` 信号 +
  `std::optional<int> AnchorY(slug)`（内部 slug → h-N →
  `QLiteHtmlWidget::anchorY`）；
- `AnchorY` 返回**文档坐标**（0 合法，表示标题就在文档最顶部），找不到
  返回 nullopt，调用方不会混淆"在顶部"与"找不到"；单位与滚动条值一致；
- `AnchorY` 在视图内按文档缓存锚点 y（首次查询一次性填充，布局变化/
  缩放/重排时失效），宿主按滚动位置反查不会把 O(n) 次 DOM 搜索压到
  每个滚动事件上；
- 当前章节由**宿主反查**：遍历 `outline()`，取最后一个 `AnchorY <= y`
  （留几像素容差——标题自带 margin，顶部标题的 y 通常大于 0）的标题；
  控件只提供 `SetCurrentAnchor()`，不做反查，避免把滚动策略塞进控件；
- `ReapplyDocument` 恢复滚动位置后显式发一次 `viewportScrolled`：值没变
  时 Qt 不发 `valueChanged`，而大纲刚被重建、选中已清空。

**控件 `MarkdownOutlineWidget`（独立文件，随库提供）**：

- `QTreeWidget`：无表头、无根装饰、按层级缩进、统一行高；
- 方法：`SetHeadings(const QVector<MarkdownHeading>&)`、
  `SetCurrentAnchor(const QString&)`；信号 `headingActivated(QString)`；
- 数据驱动，不直接依赖 `MarkdownView`；宿主把 `outlineChanged` /
  `viewportScrolled` 接到控件上（示例里演示）；
- 不写死 QSS，配色交给宿主样式体系（md_viewer 的 Fluent 风格直接生效）。

**qlitehtml 补丁 0003**：`QLiteHtmlWidget` 暴露
`int anchorY(const QString& name) const`；找不到锚点时沿用
DocumentContainer 的口径返回 **-1**，`MarkdownView::AnchorY` 原样透传。

**示例集成**：md_viewer 增加"大纲" `QDockWidget`，视图菜单加可勾选
"显示大纲"。

### 6.7 主题切换（亮 / 暗）

> 后续变更（2026-10-19）：高对比主题已移除，只保留 Light/Dark。原因是
> md_viewer 的主题选择同时驱动 Fluent 壳，而 Fluent 只有亮/暗两档，高对比
> 无法一一对应。以下表格为设计时的三主题记录，实现已按两主题收敛。

**资源**：

```
resources/themes/light.css
resources/themes/dark.css
```

qrc 别名 `:/markdown/themes/<name>.css`；`markdown_default.css` 保留别名
指向 light。`MD_MARKDOWN_CSS` 仍是最高优先级，但定位为**纯调试覆盖**：
它只换 CSS，不改变公式/Mermaid 的配色，二者混用会出现配色错位——
文档写明；测试涉及主题 CSS 时必须先清掉该环境变量。覆盖生效时
`SetTheme()` 对 CSS 无效、只切公式/Mermaid 配色，切到非 Light 主题会打
一次 `qWarning`，避免"CSS 亮、公式暗"静默发生。

**横切维度**：

| 维度 | Light | Dark |
|---|---|---|
| CSS | light.css | dark.css |
| 公式前景色 | `#24292e` | `#e6edf3` |
| Mermaid | `theme:'default'` | `theme:'dark'` |

- 公式：渲染前把 SVG 里的 `currentColor` 替换为指定颜色；如果没有
  `currentColor`，在根 `<g>` 上补 `fill`；颜色进缓存 key；
- Mermaid：`initialize` 只做一次，带**完整基础配置**（`htmlLabels:false`
  等）且**不带主题**；主题走每张图前缀的 `%%{init: ...}%%` 指令（Dark 用
  `theme:'dark'`），主题名进缓存 key；
- 代码高亮：只换 CSS；
- `SetTheme()`：更新 CSS 选择与两个 sink 捕获的颜色/主题 → 重渲染并保留
  滚动；缓存按 key 自然隔离，不清空；旧主题条目按 FIFO 淘汰（与 §6.3
  一致；内存缓存各自维持现有上限，不套磁盘的 FIFO）；
- 多视图：上下文隔离；Mermaid 主题按图生效，`initialize` 不再随视图主题
  重设，两个视图交替主题互不污染。阶段 4 实测：`%%{init}%%` 能覆盖
  initialize 的主题，且清缓存重渲后不会泄漏到后续渲染，因此采用指令方案，
  原先"同进程建议统一主题"的限制取消。

## 七、qlitehtml 补丁清单

| 编号 | 内容 | 用途 | 阶段 |
|---|---|---|---|
| 0003 | `QLiteHtmlWidget::anchorY(const QString&)` | 大纲当前章节 | 已落地 |
| 0007 | `QLiteHtmlWidget::replaceImage(const QUrl&, const QByteArray&)` | 图片懒加载（预留，未落地） | 阶段 2 |
| 0008 | `DocumentContainer::PixmapCallBack` | 公式/图表 QPixmap 直供 | 已落地 |

登记：`patches/qlitehtml/NNNN-*.patch` + `cmake/patch_dependencies.cmake`；
`3rdparty/` 不入库。

## 八、测试判据

### 行号

1. 默认关闭：现有 `<pre><code>` 断言全部保持；
2. 打开：行号 1..N、`md-code-ln` / `md-code-line` 数量正确；
3. 跨行注释：每个 `md-code-line` 单元格 `<span`/`</span>` 配平
   （按单元格，不按换行）；
4. 中间空行保留为一行、单元格含 `&#160;`；
5. 空代码块（`code.isEmpty()`）零行；`"\n"` 是一行；结尾换行不多行；
6. `CodeBlockSink` 仍收到原始源码，`copy:N` 仍在；
7. Mermaid 渲染失败回落时按代码块处理（可带行号）；
7b. 基准（**非门禁**）：1000 行代码块（表格）布局耗时记录到测试日志，
    开发机参考值约 0.4 s；不设 CI 硬阈值（机器差异大），只用于发现
    "数量级恶化"。

### 上下文与缓存契约

8a. 默认构造：`options().formula_cache_entries` / `mermaid_cache_entries`
    与实际 `max_entries` 一致（512 / 64）；
8b. `SetOptions` 调小上限后，已有条目**立即**裁到新上限；
8c. `FormulaToImgTag(" x ")` 与 `"x"` 命中同一条缓存（renders 1、
    memory_cache_hits 1）；
8d. `ClearFormulaCache()` 只清默认上下文；自定义上下文条目不受影响。

### 开关与懒加载

8. 公式关闭：`$x$` 原样出现；引擎 loaded 状态在**独立测试可执行文件**
   里断言为 false（引擎是进程级共享，测试不能依赖执行顺序）；
9. Mermaid 关闭：围栏输出 `<pre><code>`；同样用独立可执行文件断言未加载；
10. 从关到开重渲染后，`$…$` 变成 `<img>`。

### 持久缓存

11. 临时目录：写入 → 清内存缓存 → 再取命中，disk_hits +1；
12. 失败结果不落盘；
13. 容量淘汰：注入小上限（如 2 条），第 3 条写入后最旧被删；
14. 关闭开关后不产生文件；
15. 两个上下文用不同目录互不干扰。

### 诊断

16. 全新上下文查询状态不触发加载；
17. 计数：一次高亮 renders=1；失败计数不在测试里人为制造（未注册语言是
    正常回退，不计 failure），失败路径由代码评审覆盖；
18. `ResetRendererStats()` 清计数、`loaded`/`permanently_failed` 不变；
19. trace 打开后 `lcMarkdownRender` 有输出（`qInstallMessageHandler` 断言）；
    两个上下文一开一关时，只有开的那个产出日志。

### 大纲

20. 标题层级/文本/锚点顺序正确；回调为空不产出；
21. `MarkdownOutlineWidget` 条目数与层级正确，激活发出 anchor；
22. `AnchorY()` 随标题文档顺序单调递增（不是随滚动位置）；找不到锚点返回
    nullopt，文档顶部标题返回 0。

### 主题

23. 三套 CSS 都能从 qrc 读到；切换后外层 CSS 文本变化。测试必须在**首次
    渲染前** `qunsetenv("MD_MARKDOWN_CSS")`——`LoadDefaultCss` 只读一次
    该变量，进程中途清掉无效；
24. 公式颜色进 key：亮/暗生成两个条目；
25. Mermaid 主题进 key；切换后重新 initialize 且结果不同（冒烟）。

### 图片懒加载（阶段 1）

26. 关闭时行为不变；
27. 同一图片重渲染命中字节缓存（`image_cache_hits` +1）；
    mtime 变化后重新读盘（misses +1）。

## 九、实施顺序

1. **RenderContext 基础**：上下文、引擎/状态拆分、高亮 sink、大纲回调、
   便捷重载；现有行为不变；
2. **行号 + 渲染开关 + 状态接口**；
3. **诊断跟踪/转储**：缓存与后续阶段的日志都依赖它，放在缓存之前；
4. **主题**：三份 CSS、公式颜色、Mermaid 主题；
5. **持久缓存**：key 已含主题/颜色，不会二次改；
6. **大纲**：数据 + 控件 + 补丁 0003 + md_viewer dock；
7. **图片懒加载阶段 1**；阶段 2（补丁 0007）另行评估。

## 十、已确认决策与遗留问题

已确认：

- 渲染器状态按视图隔离（`MarkdownRenderContext`），引擎进程级共享；
  三个渲染器各自保留自己的 runtime，不合并；
- 持久缓存默认开启；便捷默认上下文默认关闭，避免现有测试写用户目录；
- 大纲是独立控件，宿主决定放置；
- 图片懒加载先做阶段 1，阶段 2 另评估；
- 淘汰用 FIFO + 批量清理，不在每次命中时 setFileTime；
- trace 类别进程级启用一次，输出由每上下文的标志门控。

遗留：

1. 主题枚举三种是否够用？高对比 Mermaid 的 themeVariables 需在实现时做
   像素验收（对比度、连线可见性）。
2. `MD_MARKDOWN_CSS` 与主题混用的提示方式：文档说明即可，还是检测到
   覆盖时在 trace 里警告？


## 十一、用户评审意见核实结论

用户评审（2026-10-17）提出的 7 条，加上实现后追查的 7 条，逐条核实如下；处置已并入前文。

### 1. 行号表格布局风险 —— 部分成立，前提需修正

- **表格不一定更慢**：实测（x86 relwithdebinfo、900x700、真实 CSS、
  `setHtml` 两次）1000 行 `<pre>` 737/738 ms、`<table>` 363/375 ms；
  500 行基本持平；100 行 table 略慢（39/42 vs 23/26 ms）。千行文档的
  布局成本本身就在数百毫秒级，**行号不是主因**。
- `text-indent` 有实现（`css_properties.cpp:227`、
  `render_inline_context.cpp:149-151,206-208`）；`white-space: pre-wrap`
  有实现（`el_space.cpp:11-27`、`line_box.cpp:733`）；`word-wrap` 确实
  没有（§3.2）。
- `width:1%` + `nowrap` + `pre-wrap` 的组合已出样张
  （`temp/md_code_table_sample.png`）：行号右对齐、代码列对齐、折行续行
  右移 2em，列宽稳定。
- **处置**：保留表格方案；测试增加一条**非门禁**基准（记录 1000 行耗时，
  不设 CI 硬阈值）。备选的 span + `::before` 不采用：仍需逐行块元素，
  且实测未显示表格劣势；litehtml 0.10 虽支持伪元素与 counter，但生成
  内容在选中/打印路径上不如显式文本可控。

### 2. MakeRenderCacheKey 的 qrc 初始化 —— 成立

`MakeRenderCacheKey` 内部先调 `EnsureMarkdownResources()`（幂等）。已写入
§6.3。否则静态库 qrc 未初始化，`QFile::open` 失败、hash 为空，持久缓存
永远 miss。

### 3. 便捷默认 ctx 的线程安全 —— 成立

函数级静态只保证初始化安全，不保证后续访问。已写入 §4.1/§5.2：
**便捷重载只能在 GUI 线程调用**；上下文不跨线程；异步渲染另案设计。
头文件同步加约束注释。

### 4. Mermaid 主题与引擎级 initialize 的耦合 —— 成立

随仓 mermaid 11.16.0：公开 `render` API 没有 per-call config，主题只能走
`initialize`（全局）或 `%%{init: ...}%%` 指令（per-diagram）。两个视图
交替主题会反复 initialize；缓存按主题隔离后各自命中，但首次各渲染一遍。
阶段 4 实测：per-diagram 指令能覆盖 initialize 的主题，且清缓存重渲后
不泄漏到后续渲染；已落地为"initialize 不带主题、ThemeDirective 逐图
前缀"，多视图交替主题不再反复 initialize。异步渲染时 QuickJS context
的竞争问题由"渲染器限定 GUI 线程"约束挡住，不在此轮解决。

### 5. AnchorY 的 -1 与 0 歧义 —— 成立

已改为 `std::optional<int> AnchorY()`：0 是合法文档坐标，找不到返回
nullopt。§6.6 同时写明"当前章节由宿主用 outline + AnchorY 反查（O(n)）"，
控件不做反查。

### 6. OutlineSink 与 options.outline 类型不一致 —— 成立，已修

删掉 `OutlineSink`；`AddHeadingIds` 的第三个参数直接接收
`std::function<void(const MarkdownHeading&)>`，`MarkdownToHtml` 原样转发
`options.outline`，不再有适配 lambda。代码与测试已同步。

### 7. BuildCodeBlockFrame 如何接收行号选项 —— 成立，已补

§6.1 明确：签名增加 `bool line_numbers`，关闭分支保持现有 `<pre><code>`
代码一字不改；选项由 `MarkdownRenderOptions::code_line_numbers` 经
`HighlightCodeBlocks` 传入。

### 8. 默认构造的缓存上限与 options 不一致 —— 成立，已修

`RenderStringCache` 的内部默认是 256，而 `MarkdownRenderContextOptions`
对外承诺 512/64；构造函数没调 `SetOptions`，导致 `MarkdownView` 的上下文
实际 256/256，而 `options()` 返回 512/64。已修：`MarkdownRenderContextState`
构造时按 options 同步上限；`SetOptions` 调小上限时立即 `Trim()`。

### 9. 视图注释指向全局 Clear*Cache —— 成立，已修

`LoadFile` 的注释让宿主调 `ClearFormulaCache()` / `ClearMermaidCache()`，
但这两个函数清的是进程默认上下文，对视图自己的 `context_` 无效。已修：
新增 `MarkdownView::ClearRenderCaches()` 与 `render_context()`，注释改指
它们；渲染器头文件明确旧函数只作用于默认上下文。

### 10. context_ 未暴露 —— 成立，已修

新增 `MarkdownView::render_context()`（可写/只读）与
`ClearRenderCaches()`；宿主可读 `status()`、改 `SetOptions()`（缓存目录/
容量/诊断）、`ResetStats()`。生命周期跟随 `MarkdownView`。

### 11. 公式缓存 key 未 trim —— 成立，已修

key 用未 trim 的 tex，渲染却用 `tex.trimmed()`，`"x"` 与 `" x "` 会存两份。
已修：key 与渲染输入统一用 trim 后的文本；新增测试钉住
`renders == 1 && memory_cache_hits == 1`。

### 12. EnsureThemeInitialized 失败不持久化，会反复重试 —— 成立，已修

加载失败有 `g_load_failed`，主题 initialize 失败却只 `return false`；
`MermaidToImgTag` 每次都会重试并重复 `qWarning`。已修：失败直接走
`FailLoad()`（作废引擎、一次性语义）；`FailLoad` 只在首次加载耗时为空时
写入，避免主题失败覆盖已经测到的加载耗时。头文件注释改成"渲染失败缓存
空串；载荷/主题初始化失败走一次性标志"。

### 13. ClearDocument 残留 code_blocks_ / heading_ids_ —— 成立，已修

`ClearDocument()` 只清 `markdown_`/`base_dir_`/`current_file_`，两张文档级
表残留。当前错误页没有 `copy:`/锚点，实际无害，但语义不对。已修：
`ClearDocument()` 一并清两张表（`ApplyDocument` 里的清理保留，幂等）。

### 14. diagnostics 两处存储冗余 —— 成立，已修

`MarkdownRenderContextState` 同时有 `diagnostics` 与 `options.diagnostics`，
`SetOptions`/`SetDiagnostics` 各写两处，将来容易漂移。已修：删掉
`state.diagnostics`，唯一来源是 `options.diagnostics`；`diagnostics()`
直接读它。

### 结论

14 条中 13 条成立（均已落文/落码），1 条（表格布局）风险前提被实测
修正。没有新的阻塞项，文档按此定稿。
