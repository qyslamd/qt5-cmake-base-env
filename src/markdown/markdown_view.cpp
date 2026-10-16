#include "markdown_view.h"

#include <QClipboard>
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

#include "formula_renderer.h"
#include "markdown_to_html.h"
#include "mermaid_renderer.h"
#include "qlitehtmlwidget.h"

namespace {

// "先删后建"保存的等待节奏：每 100ms 看一次，最多等 5 秒
constexpr int kReloadIntervalMs = 100;
constexpr int kReloadMaxAttempts = 50;

// 缩放：步进 1.1 倍，范围与 demo 一致；再大中文会糊、再小认不出字
constexpr qreal kZoomStep = 1.1;
constexpr qreal kZoomMin = 0.3;
constexpr qreal kZoomMax = 5.0;

// qrc 里的默认样式，进程内只读一次（qrc 的内容不会变）
const QString& DefaultCssFromResources() {
  static const QString kCss = []() -> QString {
    QFile file(QStringLiteral(":/markdown/markdown_default.css"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      // qrc 读不出来是构建问题、运行时几乎不会发生，但"样式全丢"比"丑一点"
      // 严重得多（表格/代码块/图都会失去排版），退回一份最小样式并留痕
      qWarning() << "markdown: default css missing from resources,"
                    " falling back to built-in minimal css";
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
  }();
  return kCss;
}

// MD_MARKDOWN_CSS=<文件>：存在就覆盖 qrc 里的样式——调样式不用重编，发布
// 默认关闭（与 MD_MERMAID_DUMP 同一个模式）。环境变量读一次；**文件**每次
// 现读，改完重开文档就生效，不用重启程序。设了但读不了就落回 qrc
QString LoadDefaultCss() {
  static const QString kOverridePath = qEnvironmentVariable("MD_MARKDOWN_CSS");
  if (!kOverridePath.isEmpty()) {
    QFile file(kOverridePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      return QString::fromUtf8(file.readAll());
    }
    qWarning() << "markdown: MD_MARKDOWN_CSS set but unreadable,"
               << kOverridePath;
  }
  return DefaultCssFromResources();
}

// 相对路径的图片与公式图都走这里：qlitehtml 按基准 URL 解析出路径来要数据
QByteArray ReadLocalFile(const QUrl& url) {
  // 公式图是 data URI 内联进来的（formula_renderer 产出），这里解码还原。
  // 用 data URI 而不是自定义 scheme，是因为后者会被 qlitehtml 当主机名做
  // 浏览器式扩展（实测 mj://0 → mj://0.0.0.0），回调里对不上号
  static const QString kPngPrefix = QStringLiteral("data:image/png;base64,");
  const QString text = url.toString();
  if (text.startsWith(kPngPrefix)) {
    return QByteArray::fromBase64(text.mid(kPngPrefix.size()).toLatin1());
  }

  if (!url.isLocalFile()) {
    return {};
  }
  QFile file(url.toLocalFile());
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
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

// 静态库里的 qrc 不会被自动初始化。Q_INIT_RESOURCE 展开后是一个 extern 声明
// 加一次调用，**不能放进匿名命名空间**——那样声明会绑到匿名命名空间上，链接期
// 找不到 rcc 生成的那个全局函数（src/style、src/tour 都踩过同一条）
static void InitMarkdownResources() { Q_INIT_RESOURCE(markdown_res); }

MarkdownView::MarkdownView(QWidget* parent) : QWidget(parent) {
  InitMarkdownResources();
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
  html_->setResourceHandler(ReadLocalFile);
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
  html_->setHtml(
      WrapDocument(MarkdownToHtml(
                       markdown_, FormulaToImgTag, MermaidToImgTag,
                       [this](const QString& id, const QString& code) {
                         code_blocks_.insert(id, code);
                       },
                       [this](const QString& slug, const QString& id) {
                         heading_ids_.insert(slug, id);
                       }),
                   LoadDefaultCss()));
}

void MarkdownView::ShowMessage(const QString& text) {
  html_->setHtml(WrapDocument(
      QStringLiteral("<p class=\"error\">%1</p>").arg(text.toHtmlEscaped()),
      LoadDefaultCss()));
}

void MarkdownView::ClearDocument() {
  markdown_.clear();
  base_dir_.clear();
  current_file_.clear();
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
