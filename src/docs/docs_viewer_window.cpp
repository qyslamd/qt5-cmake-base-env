#include "docs_viewer_window.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QVBoxLayout>

#include "markdown_outline_widget.h"
#include "markdown_view.h"

DocsViewerWindow::DocsViewerWindow(QWidget* parent) : QDialog(parent) {
  initUi();
  initSignals();

  QString docs_root = QDir(QCoreApplication::applicationDirPath())
                          .filePath(QStringLiteral("docs"));
  if (!QFileInfo(docs_root).isDir()) {
    docs_root = QDir::current().filePath(QStringLiteral("docs"));
  }
  if (!catalog_.Load(docs_root)) {
    showStatus(catalog_.error());
    return;
  }
  populateDocuments();
  if (doc_list_->count() > 0) {
    doc_list_->setCurrentRow(0);
  }
}

void DocsViewerWindow::initUi() {
  setObjectName(QStringLiteral("docsViewerWindow"));
  setWindowTitle(QStringLiteral("应用文档"));
  resize(1200, 800);

  auto* left = new QWidget(this);
  auto* left_layout = new QVBoxLayout(left);
  left_layout->setContentsMargins(0, 0, 8, 0);
  left_layout->addWidget(new QLabel(QStringLiteral("文档"), left));
  doc_list_ = new QListWidget(left);
  doc_list_->setObjectName(QStringLiteral("docsList"));
  left_layout->addWidget(doc_list_, 1);
  left_layout->addWidget(new QLabel(QStringLiteral("大纲"), left));
  outline_ = new MarkdownOutlineWidget(left);
  outline_->setObjectName(QStringLiteral("docsOutline"));
  outline_->setMinimumHeight(140);
  left_layout->addWidget(outline_, 1);

  auto* right = new QWidget(this);
  auto* right_layout = new QVBoxLayout(right);
  right_layout->setContentsMargins(8, 0, 0, 0);

  find_edit_ = new QLineEdit(right);
  find_edit_->setObjectName(QStringLiteral("docsFindEdit"));
  find_edit_->setPlaceholderText(QStringLiteral("在文档中查找"));
  find_edit_->setClearButtonEnabled(true);
  find_previous_button_ = new QPushButton(QStringLiteral("上一个"), right);
  find_next_button_ = new QPushButton(QStringLiteral("下一个"), right);
  zoom_out_button_ = new QPushButton(QStringLiteral("-"), right);
  zoom_in_button_ = new QPushButton(QStringLiteral("+"), right);
  zoom_reset_button_ = new QPushButton(QStringLiteral("100%"), right);
  export_button_ = new QPushButton(QStringLiteral("导出 HTML"), right);
  print_button_ = new QPushButton(QStringLiteral("打印"), right);

  auto* toolbar = new QHBoxLayout;
  toolbar->addWidget(find_edit_, 1);
  toolbar->addWidget(find_previous_button_);
  toolbar->addWidget(find_next_button_);
  toolbar->addSpacing(12);
  toolbar->addWidget(zoom_out_button_);
  toolbar->addWidget(zoom_reset_button_);
  toolbar->addWidget(zoom_in_button_);
  toolbar->addSpacing(12);
  toolbar->addWidget(export_button_);
  toolbar->addWidget(print_button_);
  right_layout->addLayout(toolbar);

  view_ = new MarkdownView(right);
  view_->setObjectName(QStringLiteral("docsMarkdownView"));
  right_layout->addWidget(view_, 1);

  status_label_ = new QLabel(QStringLiteral("正在读取文档..."), right);
  status_label_->setObjectName(QStringLiteral("docsStatusLabel"));
  right_layout->addWidget(status_label_);

  auto* splitter = new QSplitter(Qt::Horizontal, this);
  splitter->addWidget(left);
  splitter->addWidget(right);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({260, 900});

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->addWidget(splitter);

  auto* find_shortcut = new QShortcut(QKeySequence::Find, this);
  connect(find_shortcut, &QShortcut::activated, this, [this]() {
    find_edit_->setFocus();
    find_edit_->selectAll();
  });
}

void DocsViewerWindow::initSignals() {
  connect(doc_list_, &QListWidget::currentRowChanged, this,
          &DocsViewerWindow::onDocumentRowChanged);
  connect(outline_, &MarkdownOutlineWidget::headingActivated, this,
          &DocsViewerWindow::onHeadingActivated);
  connect(view_, &MarkdownView::linkClicked, this,
          &DocsViewerWindow::onLinkClicked);
  connect(view_, &MarkdownView::outlineChanged, this,
          [this]() { outline_->SetHeadings(view_->outline()); });
  connect(view_, &MarkdownView::renderFinished, this,
          &DocsViewerWindow::onRenderFinished);
  connect(find_edit_, &QLineEdit::returnPressed, this,
          &DocsViewerWindow::onFindNext);
  connect(find_previous_button_, &QPushButton::clicked, this,
          &DocsViewerWindow::onFindPrevious);
  connect(find_next_button_, &QPushButton::clicked, this,
          &DocsViewerWindow::onFindNext);
  connect(zoom_out_button_, &QPushButton::clicked, this,
          &DocsViewerWindow::onZoomOut);
  connect(zoom_in_button_, &QPushButton::clicked, this,
          &DocsViewerWindow::onZoomIn);
  connect(zoom_reset_button_, &QPushButton::clicked, this,
          &DocsViewerWindow::onZoomReset);
  connect(export_button_, &QPushButton::clicked, this,
          &DocsViewerWindow::onExportHtml);
  connect(print_button_, &QPushButton::clicked, this,
          &DocsViewerWindow::onPrint);
  connect(view_, &MarkdownView::zoomChanged, this, [this](qreal zoom) {
    zoom_reset_button_->setText(QStringLiteral("%1%").arg(qRound(zoom * 100)));
  });
}

void DocsViewerWindow::populateDocuments() {
  updating_selection_ = true;
  doc_list_->clear();
  for (const DocEntry& entry : catalog_.entries()) {
    auto* item = new QListWidgetItem(
        QStringLiteral("%1 / %2").arg(entry.category, entry.title), doc_list_);
    item->setData(Qt::UserRole, entry.relative_path);
    item->setToolTip(entry.relative_path);
  }
  updating_selection_ = false;
}

void DocsViewerWindow::onDocumentRowChanged(int row) {
  if (updating_selection_ || row < 0 || row >= catalog_.entries().size()) {
    return;
  }
  openEntry(catalog_.entries().at(row));
}

void DocsViewerWindow::openEntry(const DocEntry& entry, const QString& anchor) {
  current_path_ = entry.absolute_path;
  pending_anchor_ = anchor;
  if (!view_->LoadFile(current_path_)) {
    showStatus(QStringLiteral("打开失败: %1").arg(entry.relative_path));
    return;
  }
  showStatus(entry.relative_path);
  if (anchor.isEmpty()) {
    pending_anchor_.clear();
  }
}

void DocsViewerWindow::onHeadingActivated(const QString& anchor) {
  view_->ScrollToAnchor(anchor);
}

void DocsViewerWindow::onRenderFinished(quint64 request_id) {
  Q_UNUSED(request_id);
  if (!pending_anchor_.isEmpty()) {
    const QString anchor = pending_anchor_;
    pending_anchor_.clear();
    view_->ScrollToAnchor(anchor);
  }
}

void DocsViewerWindow::onLinkClicked(const QUrl& url) {
  if (url.scheme().isEmpty() && url.path().isEmpty() && url.hasFragment()) {
    view_->ScrollToAnchor(url.fragment());
    return;
  }

  QString path;
  QString anchor;
  if (resolveDocumentLink(url, &path, &anchor)) {
    const DocEntry* entry = findEntry(path);
    if (entry == nullptr) {
      showStatus(QStringLiteral("文档不存在: %1").arg(path));
      return;
    }
    const int row = static_cast<int>(entry - catalog_.entries().constData());
    if (doc_list_->currentRow() != row) {
      updating_selection_ = true;
      doc_list_->setCurrentRow(row);
      updating_selection_ = false;
    }
    openEntry(*entry, anchor);
    return;
  }

  if (url.scheme() == QLatin1String("http") ||
      url.scheme() == QLatin1String("https") ||
      url.scheme() == QLatin1String("mailto")) {
    QDesktopServices::openUrl(url);
    return;
  }
  if (url.isLocalFile()) {
    QDesktopServices::openUrl(url);
    return;
  }
  showStatus(QStringLiteral("忽略链接: %1").arg(url.toString()));
}

bool DocsViewerWindow::resolveDocumentLink(const QUrl& url, QString* path,
                                           QString* anchor) const {
  QUrl resolved = url;
  if (url.scheme().isEmpty()) {
    const QUrl base = QUrl::fromLocalFile(
        QFileInfo(current_path_).absolutePath() + QLatin1Char('/'));
    resolved = base.resolved(url);
  }
  if (!resolved.isLocalFile()) {
    return false;
  }
  const QString candidate =
      QFileInfo(resolved.toLocalFile()).canonicalFilePath();
  if (candidate.isEmpty() || !catalog_.Contains(candidate)) {
    return false;
  }
  const QString suffix = QFileInfo(candidate).suffix().toLower();
  if (suffix != QLatin1String("md") && suffix != QLatin1String("markdown")) {
    return false;
  }
  *path = candidate;
  *anchor = resolved.fragment();
  return true;
}

const DocEntry* DocsViewerWindow::findEntry(
    const QString& absolute_path) const {
  const QString canonical = QFileInfo(absolute_path).canonicalFilePath();
#ifdef Q_OS_WIN
  const Qt::CaseSensitivity kCase = Qt::CaseInsensitive;
#else
  const Qt::CaseSensitivity kCase = Qt::CaseSensitive;
#endif
  for (const DocEntry& entry : catalog_.entries()) {
    if (entry.absolute_path.compare(canonical, kCase) == 0) {
      return &entry;
    }
  }
  return nullptr;
}

void DocsViewerWindow::onFindNext() {
  if (find_edit_->text().isEmpty()) {
    return;
  }
  bool wrapped = false;
  const bool found =
      view_->FindText(find_edit_->text(), MarkdownView::FindNoFlag, &wrapped);
  if (!found) {
    showStatus(QStringLiteral("没有找到"));
  } else if (wrapped) {
    showStatus(QStringLiteral("已回到开头"));
  }
}

void DocsViewerWindow::onFindPrevious() {
  if (find_edit_->text().isEmpty()) {
    return;
  }
  bool wrapped = false;
  const bool found =
      view_->FindText(find_edit_->text(), MarkdownView::FindBackward, &wrapped);
  if (!found) {
    showStatus(QStringLiteral("没有找到"));
  } else if (wrapped) {
    showStatus(QStringLiteral("已回到末尾"));
  }
}

void DocsViewerWindow::onZoomIn() { view_->ZoomIn(); }
void DocsViewerWindow::onZoomOut() { view_->ZoomOut(); }
void DocsViewerWindow::onZoomReset() { view_->ResetZoom(); }

void DocsViewerWindow::onExportHtml() {
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出 HTML"), QDir::homePath(),
      QStringLiteral("HTML (*.html)"));
  if (path.isEmpty()) {
    return;
  }
  QString error;
  if (!view_->ExportHtml(path, &error)) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), error);
    return;
  }
  showStatus(QStringLiteral("已导出: %1").arg(path));
}

void DocsViewerWindow::onPrint() {
  QPrinter printer(QPrinter::HighResolution);
  QPrintDialog dialog(&printer, this);
  if (dialog.exec() == QDialog::Accepted) {
    view_->Print(&printer);
  }
}

void DocsViewerWindow::showStatus(const QString& text) {
  status_label_->setText(text);
}
