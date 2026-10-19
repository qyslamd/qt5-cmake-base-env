#ifndef FILE_MARKDOWN_MARKDOWN_VIEW_H_
#define FILE_MARKDOWN_MARKDOWN_VIEW_H_

#include <memory>
#include <optional>

#include <QHash>
#include <QString>
#include <QUrl>
#include <QVector>
#include <QWidget>

#include "markdown_types.h"

class MarkdownRenderContext;
class QFileSystemWatcher;
class QLiteHtmlWidget;
class QPrinter;

// copy: 链接（代码块"复制"按钮）→ 要复制的原文；不是 copy: 或表里查不到
// 返回空串。抽成自由函数是为了可测：物理点击 funq 驱动不了（html 是
// qlitehtml 自绘的，合成鼠标事件到不了它的链接命中判定），而"信号之后"的
// 拦截与查表是本模块的逻辑。空串 = 别动剪贴板（宁可没反应，也不能把用户
// 剪贴板里原来的东西清掉）
QString CopyTextForLink(const QUrl& url, const QHash<QString, QString>& blocks);

// 资源回调：图片/公式图的 URL → 字节。两道闸：
//  - data: URI 只认 PNG，解码量上限 16MB（恶意 md 塞 GB 级 base64 会在
//    解码那一刻吃满内存）；
//  - 本地文件必须落在 base_dir 之内（文档里写 <img src="/etc/passwd"> 或
//    file:///…/.ssh/id_rsa 不该被读出来）。比对 canonical 路径，符号链接
//    按解析后的位置算；base 为空时本地文件一律拒绝
QByteArray ReadResourceForUrl(const QUrl& url, const QString& base_dir);

// Markdown 预览控件：加载 .md、套内置样式渲染、跟随文件变化重载。
// 只做"渲染"这一件事——链接点击原样抛出（copy: 的代码块复制链接除外，那
// 是控件内部能力：查表放剪贴板，见 ApplyDocument）；本地 .md 是自开还是
// 交给系统浏览器是宿主的策略；qlitehtml 的类型不出现在本头文件里，换渲染
// 后端不动调用方。
//
// 内置交互：Ctrl+滚轮 / Ctrl+= / Ctrl+- 缩放（0.3~5.0）、Ctrl+0 复位、
// Ctrl+C 复制选中文本（QAbstractScrollArea 的默认按键处理会吞掉 Copy，
// 必须拦）。
class MarkdownView : public QWidget {
  Q_OBJECT

 public:
  // 查找选项。自定义而不用 QTextDocument::FindFlags——宿主不该为了几个
  // 枚举值把 QTextDocument 拉进编译单元
  enum FindFlag {
    FindNoFlag = 0,
    FindBackward = 1,
    FindCaseSensitively = 2,
    FindWholeWords = 4,
  };
  Q_DECLARE_FLAGS(FindFlags, FindFlag)
  explicit MarkdownView(QWidget* parent = nullptr);
  ~MarkdownView() override;

  // 从磁盘加载，成功返回 true。基准 URL 取文件所在目录，相对路径的图片与
  // 链接据此解析；失败时视图内提示原因，并回到"没有文档"的状态
  bool LoadFile(const QString& path);
  // 直接给文本。基准 URL 就是 base_dir（空 = 没有基准），不给上一份文档的目录
  // 留后门——否则相对图片会悄悄按上次那个目录解析
  void SetContent(const QString& markdown, const QString& base_dir);
  QString current_file() const { return current_file_; }
  // 两种载入都算"有文档"：LoadFile 落 current_file_，SetContent 只落
  // markdown_——只查后者的话文本直载后这里恒为 false
  bool has_document() const {
    return !current_file_.isEmpty() || !markdown_.isEmpty();
  }

  // 滚到页内锚点：用户写的是 slug（`#标题`），这里经内部的 slug→id 表换成
  // 元素再滚；查不到就原样透传（手写的 <a name="x"> 那类归 litehtml 自己找）
  void ScrollToAnchor(const QString& anchor);

  // 查找并选中命中处（找到会滚过去）。wrapped 非空时被写成"本次是否绕回了
  // 开头"（每次调用都会写——控件本体只在绕回时写，见实现），宿主拿它提示
  // "已到末尾"。返回 false = 没有命中，选区不动。
  // incremental=true 从当前选区开头找，供"边打字边找"使用；默认 false 从
  // 选区末尾找下一个，适合 F3/回车
  bool FindText(const QString& text, FindFlags flags, bool* wrapped = nullptr,
                bool incremental = false);
  // 打印/导出 PDF：QPrinter 由宿主自备（本控件不碰文件系统与输出格式）。
  // 控件按 print 媒体重排一遍再逐页绘制——分页是按页高切片的，跨页的长
  // 元素会被切在页缝上
  void Print(QPrinter* printer);

  // 代码块行号（默认关）；切换后若有文档立即重渲染并保留滚动位置
  void SetCodeLineNumbers(bool enabled);
  bool code_line_numbers() const { return code_line_numbers_; }
  // 公式/Mermaid 渲染（默认开）。关闭时不传对应 sink：$…$ 原样显示、
  // mermaid 围栏退回代码块，且不会触发 JS 载荷加载
  void SetFormulaRenderingEnabled(bool enabled);
  bool formula_rendering_enabled() const { return formula_rendering_enabled_; }
  void SetMermaidRenderingEnabled(bool enabled);
  bool mermaid_rendering_enabled() const { return mermaid_rendering_enabled_; }
  // 主题（默认 Light）：样式表、公式前景色、Mermaid 主题一起切。切换后
  // 有文档立即重渲染并保留滚动；缓存按 key 自然隔离，不清空。
  // MD_MARKDOWN_CSS 生效时 CSS 保持覆盖文件（theme 对样式表无效），只切
  // 公式/Mermaid 配色，可能配色错位——切换时打一次 qWarning
  void SetTheme(MarkdownTheme theme);
  MarkdownTheme theme() const { return theme_; }
  // 缩放（0.3~5.0）。连续调用只在停手 150ms 后应用一次，避免每档都整篇重排；
  // 换文档/查找/锚点跳转会先把挂起的缩放落地
  void SetZoomFactor(qreal factor);
  qreal zoom() const { return zoom_; }
  // 整篇 litehtml 布局的累计次数：纯高度 resize 不增长，宽度/缩放/换文档
  // 才增长。给示例与排查用，不参与渲染逻辑
  int layout_count() const;
  // 本视图的渲染状态（不触发加载）与计数清零
  MarkdownRendererStatus renderer_status() const;
  void ResetRendererStats();
  // 诊断：trace 打 qt.markdown.render；dump_directory 落中间产物。
  // 只影响观测，不改变 HTML，因此不触发重渲染
  void SetDiagnostics(const MarkdownDiagnostics& diagnostics);
  MarkdownDiagnostics diagnostics() const;

  // 本次文档的标题大纲（按出现顺序，anchor 可直接传 ScrollToAnchor）；
  // 每次 ApplyDocument 重建并发出 outlineChanged
  const QVector<MarkdownHeading>& outline() const { return outline_; }
  // 标题的文档 y 坐标（单位与垂直滚动条一致）。0 合法（标题在文档最顶部），
  // 找不到锚点返回 nullopt
  std::optional<int> AnchorY(const QString& anchor) const;

  // 本视图的渲染上下文（缓存/计数/配置）。需要改缓存目录、容量或读
  // renderer_status() 时用；生命周期跟随 MarkdownView
  MarkdownRenderContext& render_context();
  const MarkdownRenderContext& render_context() const;
  // 清本视图的公式/Mermaid 内存缓存；全局 ClearFormulaCache() 清的是
  // 进程默认上下文，对视图无效
  void ClearRenderCaches();
  // 持久化渲染缓存（默认开）：公式/Mermaid 的最终 <img> 落在
  // QStandardPaths::CacheLocation/markdown-render；SetRenderCacheDirectory
  // 可换成指定根目录。关闭只跳过磁盘读写，内存缓存照常
  void SetPersistentRenderCacheEnabled(bool enabled);
  bool persistent_render_cache_enabled() const;
  void SetRenderCacheDirectory(const QString& dir);
  // 只删本模块的 formula/ 与 mermaid/ 子目录，不动根目录下的其他内容
  void ClearPersistentRenderCache();

 signals:
  void linkClicked(const QUrl& url);
  void loadFailed(const QString& path, const QString& reason);
  // 文档重建后大纲变化（含清空文档）
  void outlineChanged();
  // 垂直滚动位置变化，宿主据此反查当前章节
  void viewportScrolled(int y);
  // 缩放请求值变化；实际重排在 150ms 内合并
  void zoomChanged(qreal factor);
  // 文档渲染完成，renderer_status() 里的引擎状态与计数已更新
  void renderStatusChanged();
  // 文件内容变化后成功重载（watchdog 路径）
  void documentReloaded();
  // 本地资源（图片）读取失败；data: URI 不计入
  void resourceLoadFailed(const QUrl& url);
  // 代码块的“复制”链接已把源码写进剪贴板
  void codeCopied();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void initUi();
  void initSignals();
  void ApplyDocument();
  // 重渲染并保留垂直滚动位置（开关切换用）
  void ReapplyDocument();
  void ShowMessage(const QString& text);
  void ClearDocument();
  void WatchCurrentFile();
  void OnFileChanged();
  void OnReloadTick();
  void SetZoom(qreal factor);
  // 定时器到点：应用最后一档缩放
  void ApplyPendingZoom();
  // 立刻应用挂起的缩放（查找/锚点跳转前调用）
  void FlushPendingZoom();
  // 锚点 y 的缓存失效：换文档时显式调用；布局变化靠 layoutCount 自动发现
  void InvalidateAnchorYCache();

  // 每视图一份渲染状态（缓存/计数/配置）；引擎是进程级共享的
  std::unique_ptr<MarkdownRenderContext> context_;
  QLiteHtmlWidget* html_ = nullptr;
  QFileSystemWatcher* watcher_ = nullptr;
  // "先删后建"保存时用来等文件回来，见 OnFileChanged
  QTimer* reload_timer_ = nullptr;
  // 缩放合并：连续滚轮/快捷键只应用最后一档
  QTimer* zoom_timer_ = nullptr;
  int reload_attempts_ = 0;
  QString markdown_;
  QString current_file_;
  QString base_dir_;
  // 代码块 id → 原始代码文本，供"复制"链接取用；每次 ApplyDocument 重建
  QHash<QString, QString> code_blocks_;
  // 标题 slug → 元素 id，供页内锚点反查；同样每次 ApplyDocument 重建
  QHash<QString, QString> heading_ids_;
  // 本次文档的标题大纲；与 heading_ids_ 同源
  QVector<MarkdownHeading> outline_;
  // 锚点 → 文档 y。litehtml 的 anchorY 是 DOM 树搜索，滚动处理器按大纲
  // 反查当前章节时不能每帧每个标题各查一次；首次查询一次性填满
  mutable QHash<QString, int> anchor_y_cache_;
  mutable bool anchor_y_cache_dirty_ = true;
  // 缓存对应的布局代数；纯高度 resize 不会改变它，缓存得以保留
  mutable int anchor_y_layout_count_ = -1;
  qreal zoom_ = 1.0;
  bool code_line_numbers_ = false;
  bool formula_rendering_enabled_ = true;
  bool mermaid_rendering_enabled_ = true;
  MarkdownTheme theme_ = MarkdownTheme::Light;
  // 转储时给每次 ApplyDocument 的源/最终 HTML 编号（仅诊断用）
  int dump_apply_index_ = 0;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MarkdownView::FindFlags)

#endif  // FILE_MARKDOWN_MARKDOWN_VIEW_H_
