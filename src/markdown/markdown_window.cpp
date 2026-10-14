#include "markdown_window.h"

#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "markdown_view.h"

namespace {

// 认哪些后缀：拖进来的、内链点到的，都用这一条判据
bool IsMarkdownFile(const QString& path) {
  return path.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive) ||
         path.endsWith(QStringLiteral(".markdown"), Qt::CaseInsensitive);
}

QString FirstMarkdownPath(const QMimeData* mime) {
  if (mime == nullptr || !mime->hasUrls()) {
    return {};
  }
  const QList<QUrl> urls = mime->urls();
  for (const QUrl& url : urls) {
    if (url.isLocalFile() && IsMarkdownFile(url.toLocalFile())) {
      return url.toLocalFile();
    }
  }
  return {};
}

}  // namespace

MarkdownWindow::MarkdownWindow(QWidget* parent) : QDialog(parent) {
  initUi();
  initSignals();
}

void MarkdownWindow::initUi() {
  setObjectName(QStringLiteral("markdownWindow"));
  setWindowTitle(QStringLiteral("Markdown"));
  setAcceptDrops(true);
  resize(900, 700);

  open_button_ = new QPushButton(QStringLiteral("打开…"), this);
  open_button_->setObjectName(QStringLiteral("markdownOpen"));

  path_label_ = new QLabel(this);
  path_label_->setObjectName(QStringLiteral("markdownPath"));
  path_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  // Ignored：长路径不撑宽窗口，让它随窗口收缩
  path_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

  auto* toolbar = new QHBoxLayout;
  toolbar->setContentsMargins(8, 8, 8, 0);
  toolbar->addWidget(open_button_);
  toolbar->addWidget(path_label_, 1);

  view_ = new MarkdownView(this);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addLayout(toolbar);
  layout->addWidget(view_, 1);
}

void MarkdownWindow::initSignals() {
  connect(open_button_, &QPushButton::clicked, this,
          &MarkdownWindow::OpenFileDialog);
  connect(view_, &MarkdownView::linkClicked, this,
          &MarkdownWindow::OnLinkClicked);
  // 重载失败时视图刻意保留原内容（免得存盘瞬间看到空白页），提示只能落在这里
  connect(view_, &MarkdownView::loadFailed, this,
          [this](const QString& path, const QString& reason) {
            path_label_->setText(QStringLiteral("%1（%2）").arg(path, reason));
          });
}

void MarkdownWindow::OpenFile(const QString& path) {
  ShowDocument(path);
  show();
  raise();
  activateWindow();
}

void MarkdownWindow::ShowDocument(const QString& path) {
  path_label_->setToolTip(path);
  if (view_->LoadFile(path)) {
    path_label_->setText(path);
    setWindowTitle(
        QStringLiteral("Markdown - %1").arg(QFileInfo(path).fileName()));
    return;
  }
  // 没打开就别挂着它的文件名：视图里已经是错误页，标题跟着说实话
  path_label_->setText(QStringLiteral("打开失败：%1").arg(path));
  setWindowTitle(QStringLiteral("Markdown"));
}

void MarkdownWindow::OpenFileDialog() {
  const QString start_dir =
      view_->has_document() ? QFileInfo(view_->current_file()).absolutePath()
                            : QString();
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("打开 Markdown 文档"), start_dir,
      QStringLiteral("Markdown 文档 (*.md *.markdown);;所有文件 (*)"));
  if (!path.isEmpty()) {
    OpenFile(path);
  }
}

void MarkdownWindow::OnLinkClicked(const QUrl& url) {
  const QString fragment = url.fragment();
  const QString path = url.isLocalFile() ? url.toLocalFile() : QString();

  // 页内锚点（[x](#y)）：qlitehtml 把纯 fragment 解析成"当前基准 URL + 片段"，
  // 而基准是文档所在**目录**——落到这里 path 就成了那个目录，不拦的话会被当成
  // 本地文件交给系统打开（表现是弹出资源管理器，文档纹丝不动）
  if (path.isEmpty()) {
    view_->ScrollToAnchor(fragment);
    return;
  }

  if (IsMarkdownFile(path)) {
    ShowDocument(path);
    view_->ScrollToAnchor(fragment);  // 跨文件带锚点：载入后滚过去
    return;
  }

  // 只放行 http/https/mailto 与真实存在的本地文件：javascript:、data:、
  // file:///…exe 这类不该递到 ShellExecute 手上
  const bool web_link = url.scheme() == QLatin1String("http") ||
                        url.scheme() == QLatin1String("https") ||
                        url.scheme() == QLatin1String("mailto");
  if (web_link || QFileInfo(path).isFile()) {
    QDesktopServices::openUrl(url);
  }
}

void MarkdownWindow::dragEnterEvent(QDragEnterEvent* event) {
  if (!FirstMarkdownPath(event->mimeData()).isEmpty()) {
    event->acceptProposedAction();
  }
}

void MarkdownWindow::dropEvent(QDropEvent* event) {
  const QString path = FirstMarkdownPath(event->mimeData());
  if (!path.isEmpty()) {
    ShowDocument(path);
    event->acceptProposedAction();
  }
}
