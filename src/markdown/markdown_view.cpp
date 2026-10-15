#include "markdown_view.h"

#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include "formula_renderer.h"
#include "markdown_to_html.h"
#include "qlitehtmlwidget.h"

namespace {

// "先删后建"保存的等待节奏：每 100ms 看一次，最多等 5 秒
constexpr int kReloadIntervalMs = 100;
constexpr int kReloadMaxAttempts = 50;

// 每次渲染都要用，读一次存下来（qrc 里的内容在进程生命周期内不会变）
const QString& LoadDefaultCss() {
  static const QString kCss = []() -> QString {
    QFile file(QStringLiteral(":/markdown/markdown_default.css"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      return {};
    }
    return QString::fromUtf8(file.readAll());
  }();
  return kCss;
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

// 析构写在 .cpp：成员里只有第三方控件的前向声明，需要完整类型才能生成
MarkdownView::~MarkdownView() = default;

void MarkdownView::initUi() {
  setObjectName(QStringLiteral("markdownView"));

  html_ = new QLiteHtmlWidget(this);
  html_->setObjectName(QStringLiteral("markdownHtml"));
  html_->setResourceHandler(ReadLocalFile);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(html_);
}

void MarkdownView::initSignals() {
  connect(html_, &QLiteHtmlWidget::linkClicked, this,
          &MarkdownView::linkClicked);

  watcher_ = new QFileSystemWatcher(this);
  connect(watcher_, &QFileSystemWatcher::fileChanged, this,
          [this](const QString&) { OnFileChanged(); });

  // 不是 singleShot：文件被删后要持续等它回来，见 OnFileChanged
  reload_timer_ = new QTimer(this);
  reload_timer_->setInterval(kReloadIntervalMs);
  connect(reload_timer_, &QTimer::timeout, this, &MarkdownView::OnReloadTick);
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
  if (!anchor.isEmpty()) {
    html_->scrollToAnchor(anchor);
  }
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
  // 公式渲染以回调注入：markdown 模块不依赖公式引擎，这里把它接上
  html_->setHtml(WrapDocument(MarkdownToHtml(markdown_, FormulaToImgTag),
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
