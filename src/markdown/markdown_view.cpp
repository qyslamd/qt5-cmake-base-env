#include "markdown_view.h"

#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QScrollBar>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "code_highlighter.h"
#include "formula_renderer.h"
#include "markdown_render_context.h"
#include "markdown_resources.h"
#include "markdown_theme.h"
#include "markdown_to_html.h"
#include "markdown_types.h"
#include "mermaid_renderer.h"
#include "qlitehtmlwidget.h"
#include "render_cache.h"
#include "render_diagnostics.h"

namespace {

// "先删后建"保存的等待节奏：每 100ms 看一次，最多等 5 秒
constexpr int kReloadIntervalMs = 100;
constexpr int kReloadMaxAttempts = 50;

// 缩放：步进 1.1 倍，范围与 demo 一致；再大中文会糊、再小认不出字
constexpr qreal kZoomStep = 1.1;
constexpr qreal kZoomMin = 0.3;
constexpr qreal kZoomMax = 5.0;

// qrc 读不出来是构建问题、运行时几乎不会发生，但"样式全丢"比"丑一点"
// 严重得多（表格/代码块/图都会失去排版），退回一份最小样式并留痕
QString LoadCssResource(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "markdown: theme css missing from resources:" << path;
    return QStringLiteral(
        "body { font-family: sans-serif; font-size: 14px;"
        " line-height: 1.6; margin: 16px; }\n"
        "pre, code { font-family: monospace; }\n"
        "pre { background: #f6f8fa; padding: 8px; overflow-x: auto; }\n"
        "table { border-collapse: collapse; }\n"
        "table th, table td { border: 1px solid #d0d7de;"
        " padding: 4px 8px; }\n"
        "img { max-width: 100%; }\n"
        ".math-display { display: block; margin: 10px auto; }\n"
        ".mermaid-wrap { text-align: center; margin: 12px 0; }\n"
        "img.mermaid { max-width: 100%; height: auto; }\n");
  }
  return QString::fromUtf8(file.readAll());
}

// qrc 里的主题样式，每个主题只读一次（qrc 的内容不会变）
const QString& DefaultCssFromResources(MarkdownTheme theme) {
  static const QString kCss[kMarkdownThemeCount] = {
      LoadCssResource(ThemeCssResource(MarkdownTheme::Light)),
      LoadCssResource(ThemeCssResource(MarkdownTheme::Dark)),
      LoadCssResource(ThemeCssResource(MarkdownTheme::HighContrast)),
  };
  return kCss[ThemeIndex(theme)];
}

QString CssOverridePath() {
  static const QString kPath = qEnvironmentVariable("MD_MARKDOWN_CSS");
  return kPath;
}

// 覆盖生效时主题只切公式/Mermaid，CSS 不动；非 Light 主题会静默错位，
// 所以切换时提示一次（每个进程一次，别刷屏）
void WarnThemeCssMismatch() {
  static bool warned = false;
  if (warned) {
    return;
  }
  warned = true;
  qWarning() << "markdown: MD_MARKDOWN_CSS overrides theme css;"
                " SetTheme only changes formula/mermaid colors";
}

// MD_MARKDOWN_CSS=<文件>：存在就覆盖 qrc 里的样式——调样式不用重编，发布
// 默认关闭（与 MD_MERMAID_DUMP 同一个模式）。环境变量读一次；**文件**按
// 变更时间缓存——没变不读盘（每次 ApplyDocument 现读是白读），变了立即
// 跟上，"改完重开文档就生效"的调样式工作流不破坏。设了但读不了就落回 qrc。
// 覆盖生效时 theme 参数被忽略：只换 CSS，不换公式/Mermaid 配色
QString LoadDefaultCss(MarkdownTheme theme) {
  const QString override_path = CssOverridePath();
  if (!override_path.isEmpty()) {
    static QString cached_css;
    static QDateTime cached_modified;
    static qint64 cached_size = -1;
    static bool have_cached = false;
    const QFileInfo info(override_path);
    const QDateTime modified = info.lastModified();
    if (!have_cached || modified != cached_modified ||
        info.size() != cached_size) {
      QFile file(override_path);
      if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "markdown: MD_MARKDOWN_CSS set but unreadable,"
                   << override_path;
        // 失败态不缓存，文件回来了直接用
        return DefaultCssFromResources(theme);
      }
      cached_css = QString::fromUtf8(file.readAll());
      cached_modified = modified;
      cached_size = info.size();
      have_cached = true;
    }
    return cached_css;
  }
  return DefaultCssFromResources(theme);
}

QString WrapDocument(const QString& body, const QString& css) {
  return QStringLiteral(
             "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
             "<style>%1</style></head><body>%2</body></html>")
      .arg(css, body);
}

bool ReadMarkdownFile(const QString& path, QString* text) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }
  *text = QString::fromUtf8(file.readAll());
  return true;
}

}  // namespace

// 实现见头文件注释（两道闸）。留在自由函数里是为了可测：资源回调是
// qlitehtml 的私有路径，单测从这层进去就够了
QByteArray ReadResourceForUrl(const QUrl& url, const QString& base_dir) {
  // 公式图是 data URI 内联进来的（formula_renderer 产出），这里解码还原。
  // 用 data URI 而不是自定义 scheme，是因为后者会被 qlitehtml 当主机名做
  // 浏览器式扩展（实测 mj://0 → mj://0.0.0.0），回调里对不上号
  static const QString kPngPrefix = QStringLiteral("data:image/png;base64,");
  const QString text = url.toString();
  if (text.startsWith(kPngPrefix)) {
    // 编码后长度先卡一刀：解码后再量就晚了（内存已经吃进去）
    constexpr int kMaxDecodedBytes = 16 * 1024 * 1024;
    const int encoded_len = text.size() - kPngPrefix.size();
    if (encoded_len > (kMaxDecodedBytes / 3 + 1) * 4) {
      qWarning() << "markdown: data URI exceeds the 16MB decode limit";
      return {};
    }
    return QByteArray::fromBase64(text.mid(kPngPrefix.size()).toLatin1());
  }

  if (!url.isLocalFile() || base_dir.isEmpty()) {
    return {};
  }
  const QString base = QFileInfo(base_dir).canonicalFilePath();
  if (base.isEmpty()) {
    return {};
  }
  const QString path = QFileInfo(url.toLocalFile()).canonicalFilePath();
  if (path.isEmpty()) {
    return {};
  }
#ifdef Q_OS_WIN
  const Qt::CaseSensitivity kCase = Qt::CaseInsensitive;
#else
  const Qt::CaseSensitivity kCase = Qt::CaseSensitive;
#endif
  if (path.compare(base, kCase) != 0 &&
      !path.startsWith(base + QLatin1Char('/'), kCase)) {
    qWarning() << "markdown: local resource outside the document dir, rejected:"
               << path;
    return {};
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}

MarkdownView::MarkdownView(QWidget* parent)
    : QWidget(parent), context_(new MarkdownRenderContext) {
  EnsureMarkdownResources();
  initUi();
  initSignals();
}

QString CopyTextForLink(const QUrl& url,
                        const QHash<QString, QString>& blocks) {
  if (url.scheme() != QLatin1String("copy")) {
    return QString();
  }
  return blocks.value(url.path());
}

// 析构写在 .cpp：成员里只有第三方控件的前向声明，需要完整类型才能生成
MarkdownView::~MarkdownView() = default;

void MarkdownView::initUi() {
  setObjectName(QStringLiteral("markdownView"));

  html_ = new QLiteHtmlWidget(this);
  html_->setObjectName(QStringLiteral("markdownHtml"));
  html_->setResourceHandler([this](const QUrl& url) {
    const QByteArray data = ReadResourceForUrl(url, base_dir_);
    const MarkdownDiagnostics diagnostics = context_->diagnostics();
    if (diagnostics.trace) {
      // data URI 的字符串就是整张 PNG，日志里不展开
      const QString label = url.scheme() == QLatin1String("data")
                                ? QStringLiteral("data URI")
                                : url.toString();
      TraceRender(
          diagnostics,
          QStringLiteral("resource %1: %2 bytes").arg(label).arg(data.size()));
    }
    return data;
  });
  // 缩放快捷键与 Ctrl+C 要做成控件的内置能力：Wheel 发到 viewport、
  // KeyPress 发到 widget 本身，两处都装（见 eventFilter）
  html_->setFocusPolicy(Qt::StrongFocus);
  html_->installEventFilter(this);
  html_->viewport()->installEventFilter(this);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(html_);
}

void MarkdownView::initSignals() {
  // copy: 是代码块的"复制"链接（渲染层产出），在这里就地处理、不往宿主抛：
  // 它不是导航请求，宿主不需要知道
  connect(html_, &QLiteHtmlWidget::linkClicked, this, [this](const QUrl& url) {
    if (url.scheme() == QLatin1String("copy")) {
      const QString text = CopyTextForLink(url, code_blocks_);
      if (!text.isEmpty()) {
        QGuiApplication::clipboard()->setText(text);
      }
      return;
    }
    emit linkClicked(url);
  });

  watcher_ = new QFileSystemWatcher(this);
  connect(watcher_, &QFileSystemWatcher::fileChanged, this,
          [this](const QString&) { OnFileChanged(); });

  // 不是 singleShot：文件被删后要持续等它回来，见 OnFileChanged
  reload_timer_ = new QTimer(this);
  reload_timer_->setInterval(kReloadIntervalMs);
  connect(reload_timer_, &QTimer::timeout, this, &MarkdownView::OnReloadTick);

  // 缩放的键盘快捷键走 QShortcut（WindowShortcut）：eventFilter 那条路要求
  // 焦点先在预览上，而查看窗口刚打开时焦点不在（实测 Ctrl+= 不生效），
  // "窗口激活就能按"才是查看器该有的行为
  auto add_zoom_shortcut = [this](const QKeySequence& keys, qreal factor,
                                  bool reset) {
    auto* shortcut = new QShortcut(keys, this);
    shortcut->setContext(Qt::WindowShortcut);
    connect(shortcut, &QShortcut::activated, this,
            [this, factor, reset] { SetZoom(reset ? 1.0 : zoom_ * factor); });
  };
  add_zoom_shortcut(QKeySequence::ZoomIn, kZoomStep, false);
  add_zoom_shortcut(QKeySequence::ZoomOut, 1.0 / kZoomStep, false);
  add_zoom_shortcut(QKeySequence(QStringLiteral("Ctrl+=")), kZoomStep, false);
  add_zoom_shortcut(QKeySequence(QStringLiteral("Ctrl+-")), 1.0 / kZoomStep,
                    false);
  add_zoom_shortcut(QKeySequence(QStringLiteral("Ctrl+0")), 1.0, true);
}

bool MarkdownView::eventFilter(QObject* watched, QEvent* event) {
  const bool from_html = watched == html_ || watched == html_->viewport();
  if (from_html && event->type() == QEvent::Wheel) {
    auto* wheel = static_cast<QWheelEvent*>(event);
    if (wheel->modifiers().testFlag(Qt::ControlModifier)) {
      const int delta = wheel->angleDelta().y();
      if (delta != 0) {
        SetZoom(zoom_ * (delta > 0 ? kZoomStep : 1.0 / kZoomStep));
        return true;  // 拦下：不拦会同时滚页面
      }
    }
  }
  if (from_html && event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    // QAbstractScrollArea 的默认按键处理会把 Copy 吞掉，选中文本复制不了。
    // 键盘缩放不在这里做——那条路要求焦点先在预览上，走 QShortcut（见
    // initSignals）
    if (key->matches(QKeySequence::Copy)) {
      const QString selected = html_->selectedText();
      if (!selected.isEmpty()) {
        QGuiApplication::clipboard()->setText(selected);
        return true;
      }
    }
  }
  return QWidget::eventFilter(watched, event);
}

void MarkdownView::SetZoom(qreal factor) {
  const qreal next = qBound(kZoomMin, factor, kZoomMax);
  if (qFuzzyCompare(next, zoom_)) {
    return;
  }
  zoom_ = next;
  html_->setZoomFactor(zoom_);
}

bool MarkdownView::LoadFile(const QString& path) {
  QString text;
  if (!ReadMarkdownFile(path, &text)) {
    ClearDocument();
    ShowMessage(QStringLiteral("无法打开文件：%1").arg(path));
    emit loadFailed(path, QStringLiteral("无法打开文件"));
    return false;
  }

  // 换文件才丢缓存：同一文件的重载（watchdog）要留住复用价值；SetContent
  // 是编辑器路径，每次清会把"改一个字重算全部公式"的代价请回来——那条路由
  // 宿主自己按需调 ClearRenderCaches()（或
  // render_context().ClearMemoryCaches()）。全局 ClearFormulaCache() 清的是
  // 进程默认上下文，对视图无效
  if (path != current_file_) {
    context_->ClearMemoryCaches();
  }
  markdown_ = text;
  current_file_ = path;
  base_dir_ = QFileInfo(path).absolutePath();
  WatchCurrentFile();
  ApplyDocument();
  return true;
}

void MarkdownView::SetContent(const QString& markdown,
                              const QString& base_dir) {
  ClearDocument();  // 先清空（它会清 markdown_/base_dir_），再落新内容
  markdown_ = markdown;
  base_dir_ = base_dir;
  ApplyDocument();
}

void MarkdownView::ScrollToAnchor(const QString& anchor) {
  if (anchor.isEmpty()) {
    return;
  }
  // 用户写的锚点是 slug（可能是 CJK），元素的 id 是纯 ASCII 的 h-N——查表
  // 换过去（表在 ApplyDocument 里建好）。查不到就原样透传：手写的
  // <a name="x"> 这类锚点还得靠 litehtml 自己找
  html_->scrollToAnchor(heading_ids_.value(anchor, anchor));
}

bool MarkdownView::FindText(const QString& text, FindFlags flags,
                            bool* wrapped) {
  if (text.isEmpty()) {
    return false;  // 空串不是"找全部"，是误用；选区与滚动都别动
  }
  // 控件只在"绕回"时写 wrapped，没绕回就原样留着——宿主拿到的会是个没初始化的
  // 值。这里先清零，把契约补成"每次调用都有效"
  if (wrapped != nullptr) {
    *wrapped = false;
  }
  QTextDocument::FindFlags qt_flags;
  if (flags & FindFlag::FindBackward) {
    qt_flags |= QTextDocument::FindBackward;
  }
  if (flags & FindFlag::FindCaseSensitively) {
    qt_flags |= QTextDocument::FindCaseSensitively;
  }
  if (flags & FindFlag::FindWholeWords) {
    qt_flags |= QTextDocument::FindWholeWords;
  }
  // incremental 固定 false：本仓没有"边打字边找"的 UI，省得宿主以为它有
  return html_->findText(text, qt_flags, /*incremental=*/false, wrapped);
}

void MarkdownView::Print(QPrinter* printer) {
  if (printer == nullptr) {
    return;
  }
  html_->print(printer);
}

MarkdownRenderContext& MarkdownView::render_context() { return *context_; }

const MarkdownRenderContext& MarkdownView::render_context() const {
  return *context_;
}

void MarkdownView::ClearRenderCaches() { context_->ClearMemoryCaches(); }

void MarkdownView::SetPersistentRenderCacheEnabled(bool enabled) {
  MarkdownRenderContextOptions options = context_->options();
  if (options.persistent_cache_enabled == enabled) {
    return;
  }
  options.persistent_cache_enabled = enabled;
  context_->SetOptions(options);
}

bool MarkdownView::persistent_render_cache_enabled() const {
  return context_->options().persistent_cache_enabled;
}

void MarkdownView::SetRenderCacheDirectory(const QString& dir) {
  MarkdownRenderContextOptions options = context_->options();
  if (options.cache_directory == dir) {
    return;
  }
  options.cache_directory = dir;
  context_->SetOptions(options);
}

void MarkdownView::ClearPersistentRenderCache() {
  ClearRenderCacheDirectory(MakeRenderCacheRootConfig(context_->options()));
}

void MarkdownView::SetCodeLineNumbers(bool enabled) {
  if (code_line_numbers_ == enabled) {
    return;
  }
  code_line_numbers_ = enabled;
  ReapplyDocument();
}

void MarkdownView::SetFormulaRenderingEnabled(bool enabled) {
  if (formula_rendering_enabled_ == enabled) {
    return;
  }
  formula_rendering_enabled_ = enabled;
  ReapplyDocument();
}

void MarkdownView::SetMermaidRenderingEnabled(bool enabled) {
  if (mermaid_rendering_enabled_ == enabled) {
    return;
  }
  mermaid_rendering_enabled_ = enabled;
  ReapplyDocument();
}

void MarkdownView::SetTheme(MarkdownTheme theme) {
  if (theme_ == theme) {
    return;
  }
  if (theme != MarkdownTheme::Light && !CssOverridePath().isEmpty()) {
    WarnThemeCssMismatch();
  }
  theme_ = theme;
  ReapplyDocument();
}

MarkdownRendererStatus MarkdownView::renderer_status() const {
  return context_->status();
}

void MarkdownView::ResetRendererStats() { context_->ResetStats(); }

void MarkdownView::SetDiagnostics(const MarkdownDiagnostics& diagnostics) {
  context_->SetDiagnostics(diagnostics);
}

MarkdownDiagnostics MarkdownView::diagnostics() const {
  return context_->diagnostics();
}

void MarkdownView::ReapplyDocument() {
  if (!has_document()) {
    return;
  }
  const int scroll = html_->verticalScrollBar()->value();
  ApplyDocument();
  html_->verticalScrollBar()->setValue(scroll);
}

void MarkdownView::ApplyDocument() {
  // 先 setUrl 再 setHtml：setHtml 会立刻渲染，顺序反了首帧的相对图片与相对
  // 链接会按错误的基准解析。没有基准时也显式清掉，别让上一份文档的目录留下来
  QUrl base;
  if (!base_dir_.isEmpty()) {
    base = QUrl::fromLocalFile(base_dir_);
    base.setPath(base.path() + QLatin1Char('/'));
  }
  html_->setUrl(base);
  // 公式与图表渲染以回调注入：markdown 模块不依赖渲染引擎，这里把它接上。
  // 代码块表与锚点表每次重建——旧文档的表不能留到新文档里（一个是 copy:
  // 链接的唯一数据源，串了会复制出上一份文档的代码；另一个会让 #锚点 跳到
  // 上一份文档的元素上）
  code_blocks_.clear();
  heading_ids_.clear();
  MarkdownRenderOptions options;
  options.code_line_numbers = code_line_numbers_;
  options.highlight = [this](const QString& code, const QString& language) {
    return HighlightCode(*context_, code, language);
  };
  // 关闭时不传对应 sink：MarkdownToHtml 会整段跳过抽取，也不会有任何
  // JS 载荷加载
  FormulaSink formula_sink;
  if (formula_rendering_enabled_) {
    formula_sink = [this](const QString& tex, bool display) {
      return FormulaToImgTag(*context_, tex, display,
                             ThemeFormulaColor(theme_));
    };
  }
  MermaidSink mermaid_sink;
  if (mermaid_rendering_enabled_) {
    mermaid_sink = [this](const QString& code) {
      return MermaidToImgTag(*context_, code, theme_);
    };
  }
  const QString final_html =
      WrapDocument(MarkdownToHtml(
                       markdown_, formula_sink, mermaid_sink,
                       [this](const QString& id, const QString& code) {
                         code_blocks_.insert(id, code);
                       },
                       [this](const QString& slug, const QString& id) {
                         heading_ids_.insert(slug, id);
                       },
                       options),
                   LoadDefaultCss(theme_));

  const MarkdownDiagnostics diagnostics = context_->diagnostics();
  if (diagnostics.trace) {
    TraceRender(diagnostics,
                QStringLiteral("ApplyDocument: source=%1 chars, html=%2 chars")
                    .arg(markdown_.size())
                    .arg(final_html.size()));
  }
  // 每次 ApplyDocument 一组独立序号：与 formula/N-*、mermaid/N-* 的累积
  // 语义一致，不会互相覆盖
  if (DumpEnabled(diagnostics, QStringLiteral("apply/source.md"))) {
    const int index = ++dump_apply_index_;
    DumpRenderArtifact(diagnostics,
                       QStringLiteral("apply/%1-source.md").arg(index),
                       markdown_.toUtf8());
    DumpRenderArtifact(diagnostics,
                       QStringLiteral("apply/%1-final.html").arg(index),
                       final_html.toUtf8());
  }
  html_->setHtml(final_html);
}

void MarkdownView::ShowMessage(const QString& text) {
  html_->setHtml(WrapDocument(
      QStringLiteral("<p class=\"error\">%1</p>").arg(text.toHtmlEscaped()),
      LoadDefaultCss(theme_)));
}

void MarkdownView::ClearDocument() {
  markdown_.clear();
  base_dir_.clear();
  current_file_.clear();
  // 文档级表也要清：错误页现在没有 copy:/锚点，但"清空文档"不能留上一份
  // 文档的代码块与锚点，否则将来错误页加链接会复制到旧代码
  code_blocks_.clear();
  heading_ids_.clear();
  reload_attempts_ = 0;
  if (reload_timer_ != nullptr) {
    reload_timer_->stop();
  }
  if (watcher_ != nullptr && !watcher_->files().isEmpty()) {
    watcher_->removePaths(watcher_->files());
  }
}

void MarkdownView::WatchCurrentFile() {
  if (watcher_ == nullptr) {
    return;
  }
  if (!watcher_->files().isEmpty()) {
    watcher_->removePaths(watcher_->files());
  }
  if (!current_file_.isEmpty() && !watcher_->addPath(current_file_)) {
    // 监视不上（文件不存在、或系统 inotify/watch 名额满了）就等于没有热重载，
    // 说一声比静默失效强
    emit loadFailed(current_file_,
                    QStringLiteral("无法监视文件变化，热重载不可用"));
  }
}

void MarkdownView::OnFileChanged() {
  if (current_file_.isEmpty()) {
    return;
  }

  // 编辑器保存常是"先删后建"，watcher 会因此丢掉这个文件；等它落盘再挂回去。
  // 单次延时重试在慢盘、大文件、改名保存的编辑器上会扑空，而扑空之后就再没有
  // 任何机会挂回去（热重载从此静默失效），所以这里是有界轮询
  if (watcher_ != nullptr && !watcher_->files().contains(current_file_)) {
    if (!reload_timer_->isActive()) {
      reload_attempts_ = 0;
      reload_timer_->start();
    }
    return;
  }

  // 重载读不到内容时保留原样，免得存盘的一瞬间看到空白页。滚动位置也留着：
  // 一次保存就把人弹回页首，看长文档时很难受
  QString text;
  if (!ReadMarkdownFile(current_file_, &text)) {
    emit loadFailed(current_file_, QStringLiteral("重载失败，保留原内容"));
    return;
  }
  markdown_ = text;
  const int scroll = html_->verticalScrollBar()->value();
  ApplyDocument();
  html_->verticalScrollBar()->setValue(scroll);
}

void MarkdownView::OnReloadTick() {
  if (current_file_.isEmpty()) {
    reload_timer_->stop();
    return;
  }
  if (QFile::exists(current_file_)) {
    reload_timer_->stop();
    LoadFile(current_file_);
    return;
  }
  if (++reload_attempts_ >= kReloadMaxAttempts) {
    reload_timer_->stop();
    emit loadFailed(current_file_, QStringLiteral("文件已不在，停止监视"));
  }
}
