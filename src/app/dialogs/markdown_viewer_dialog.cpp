#include "dialogs/markdown_viewer_dialog.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "markdown_view.h"

namespace {

constexpr char kSample[] =
    "# qlitemd MarkdownView\n"
    "\n"
    "这是通过 `add_subdirectory` 接入 base env 的 Qt 控件。\n"
    "\n"
    "> [!note] MPE Callout\n"
    "> 标记后同行文字作为标题，下一行是正文。\n"
    "\n"
    "## 能力\n"
    "\n"
    "- GFM 表格、任务列表、脚注[^1]\n"
    "- tree-sitter 代码高亮\n"
    "- Mermaid 与公式（后端可用时）\n"
    "- 查找、缩放、导出\n"
    "\n"
    "```cpp\n"
    "#include <QApplication>\n"
    "\n"
    "int main(int argc, char** argv) {\n"
    "  QApplication app(argc, argv);\n"
    "  return app.exec();\n"
    "}\n"
    "```\n"
    "\n"
    "| 组件 | 状态 |\n"
    "| --- | --- |\n"
    "| MarkdownView | 已嵌入 |\n"
    "| Ribbon | 已接线 |\n"
    "\n"
    "[^1]: 这是脚注。\n";

}  // namespace

MarkdownViewerDialog::MarkdownViewerDialog(QWidget* parent) : QDialog(parent) {
  initUi();
  initSignals();
}

void MarkdownViewerDialog::initUi() {
  setObjectName(QStringLiteral("markdownViewerDialog"));
  setWindowTitle(QStringLiteral("Markdown 查看器"));
  resize(1100, 800);

  view_ = new MarkdownView(this);
  view_->setObjectName(QStringLiteral("markdownViewer"));
  path_label_ = new QLabel(QStringLiteral("内置示例"), this);
  path_label_->setObjectName(QStringLiteral("markdownPathLabel"));
  path_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  open_button_ = new QPushButton(QStringLiteral("打开 Markdown..."), this);
  open_button_->setObjectName(QStringLiteral("markdownOpenButton"));

  auto* toolbar = new QHBoxLayout;
  toolbar->addWidget(open_button_);
  toolbar->addWidget(path_label_, 1);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(8);
  layout->addLayout(toolbar);
  layout->addWidget(view_, 1);

  view_->SetContent(QString::fromUtf8(kSample), QString());
}

void MarkdownViewerDialog::initSignals() {
  connect(open_button_, &QPushButton::clicked, this,
          &MarkdownViewerDialog::onOpenFile);
  connect(view_, &MarkdownView::linkClicked, this, [this](const QUrl& url) {
    if (url.scheme().isEmpty() && url.hasFragment()) {
      view_->ScrollToAnchor(url.fragment());
      return;
    }
    if (url.scheme() == QLatin1String("http") ||
        url.scheme() == QLatin1String("https")) {
      QDesktopServices::openUrl(url);
    }
  });
}

void MarkdownViewerDialog::OpenFile(const QString& path) {
  if (path.isEmpty()) {
    return;
  }
  if (view_->LoadFile(path)) {
    path_label_->setText(QFileInfo(path).absoluteFilePath());
  } else {
    path_label_->setText(QStringLiteral("打开失败: %1").arg(path));
  }
}

void MarkdownViewerDialog::onOpenFile() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("打开 Markdown"), QDir::homePath(),
      QStringLiteral("Markdown (*.md *.markdown);;所有文件 (*)"));
  if (!path.isEmpty()) {
    OpenFile(path);
  }
}
