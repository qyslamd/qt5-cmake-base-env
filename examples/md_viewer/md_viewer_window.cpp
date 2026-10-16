#include "md_viewer_window.h"

#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileInfoList>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

#include <fluentwindowframe.h>

#include "markdown_view.h"

namespace {

// 与模块的 IsMarkdownFile 一个口径：目录里认这两种后缀
const QStringList& MarkdownFilters() {
  static const QStringList kFilters{QStringLiteral("*.md"),
                                    QStringLiteral("*.markdown")};
  return kFilters;
}

}  // namespace

MdViewerWindow::MdViewerWindow(QWidget* parent) : QMainWindow(parent) {
  createActions();
  initUi();
  initSignals();
}

void MdViewerWindow::createActions() {
  open_file_action_ = new QAction(QStringLiteral("打开文件…"), this);
  open_file_action_->setShortcut(QKeySequence::Open);
  open_dir_action_ = new QAction(QStringLiteral("打开目录…"), this);
  quit_action_ = new QAction(QStringLiteral("退出"), this);
  quit_action_->setShortcut(QKeySequence::Quit);
  about_action_ = new QAction(QStringLiteral("关于"), this);
}

void MdViewerWindow::initUi() {
  setWindowTitle(QStringLiteral("Markdown 查看器"));
  resize(1100, 800);

  // 渲染全部由模块负责：加载、样式、缩放、锚点、代码块复制都是它的内置能力
  view_ = new MarkdownView(this);
  setCentralWidget(view_);

  QMenu* file_menu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
  file_menu->addAction(open_file_action_);
  file_menu->addAction(open_dir_action_);
  file_menu->addSeparator();
  file_menu->addAction(quit_action_);

  QMenu* view_menu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
  QMenu* help_menu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
  help_menu->addAction(about_action_);

  // 目录里扫到的 Markdown 铺在这：显示文件名，UserRole 存全路径
  auto* dock = new QDockWidget(QStringLiteral("文档"), this);
  dock->setObjectName(QStringLiteral("documentDock"));
  file_list_ = new QListWidget(dock);
  file_list_->setAlternatingRowColors(true);
  dock->setWidget(file_list_);
  addDockWidget(Qt::LeftDockWidgetArea, dock);
  view_menu->addAction(dock->toggleViewAction());

  path_label_ = new QLabel(this);
  statusBar()->addPermanentWidget(path_label_);

  // 无边框壳 + Fluent 标题栏；菜单栏挂进标题栏（官方 Gallery 与 demo_app 同款）
  fluent_frame_ = new FluentWindowFrame(this, this);
  fluent_frame_->installChromeHeader(menuBar());
}

void MdViewerWindow::initSignals() {
  connect(open_file_action_, &QAction::triggered, this,
          &MdViewerWindow::OpenFileDialog);
  connect(open_dir_action_, &QAction::triggered, this,
          &MdViewerWindow::OpenDirectoryDialog);
  connect(quit_action_, &QAction::triggered, this, &QWidget::close);
  connect(about_action_, &QAction::triggered, this, [this] {
    QMessageBox::about(this, QStringLiteral("关于"),
                       QStringLiteral("Markdown 查看器——examples 里的宿主示例，"
                                      "渲染由 src/markdown 提供。"));
  });
  // 点列表换文档。UserRole 存的是全路径
  connect(file_list_, &QListWidget::itemClicked, this,
          [this](QListWidgetItem* item) {
            ShowFile(item->data(Qt::UserRole).toString());
          });
  connect(view_, &MarkdownView::loadFailed, this,
          [this](const QString& path, const QString& reason) {
            statusBar()->showMessage(
                QStringLiteral("打开失败：%1（%2）")
                    .arg(QFileInfo(path).fileName(), reason),
                5000);
          });
}

void MdViewerWindow::OpenPath(const QString& path) {
  const QFileInfo info(path);
  if (info.isDir()) {
    RefreshDirectoryListing(info.absoluteFilePath());
    if (file_list_->count() > 0) {
      ShowFile(file_list_->item(0)->data(Qt::UserRole).toString());
    }
    return;
  }
  ShowFile(info.absoluteFilePath());
}

void MdViewerWindow::OpenFileDialog() {
  const QString start_dir =
      listed_dir_.isEmpty() ? QDir::homePath() : listed_dir_;
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("打开 Markdown"),
      start_dir, QStringLiteral("Markdown 文档 (*.md *.markdown);;所有文件 (*)"));
  if (!path.isEmpty()) {
    ShowFile(path);
  }
}

void MdViewerWindow::OpenDirectoryDialog() {
  const QString dir = QFileDialog::getExistingDirectory(
      this, QStringLiteral("打开目录"),
      listed_dir_.isEmpty() ? QDir::homePath() : listed_dir_);
  if (!dir.isEmpty()) {
    OpenPath(dir);
  }
}

void MdViewerWindow::RefreshDirectoryListing(const QString& dir) {
  file_list_->clear();
  const QFileInfoList files = QDir(dir).entryInfoList(
      MarkdownFilters(), QDir::Files, QDir::Name | QDir::IgnoreCase);
  for (const QFileInfo& info : files) {
    auto* item = new QListWidgetItem(info.fileName(), file_list_);
    item->setData(Qt::UserRole, info.absoluteFilePath());
    item->setToolTip(info.absoluteFilePath());
  }
  listed_dir_ = dir;
  statusBar()->showMessage(
      QStringLiteral("%1：%2 篇 Markdown").arg(dir).arg(files.size()), 5000);
}

void MdViewerWindow::ShowFile(const QString& path) {
  const QFileInfo info(path);
  // 单篇打开时顺手把同目录的兄弟列出来——侧栏不该只对"打开目录"生效
  if (info.absolutePath() != listed_dir_) {
    RefreshDirectoryListing(info.absolutePath());
  }
  if (!view_->LoadFile(info.absoluteFilePath())) {
    return;  // 视图里已经是错误页，状态栏的消息由 loadFailed 负责
  }
  setWindowTitle(
      QStringLiteral("Markdown 查看器 - %1").arg(info.fileName()));
  path_label_->setText(info.fileName());
  path_label_->setToolTip(info.absoluteFilePath());
  SelectInList(info.absoluteFilePath());
}

void MdViewerWindow::SelectInList(const QString& path) {
  for (int row = 0; row < file_list_->count(); ++row) {
    QListWidgetItem* item = file_list_->item(row);
    if (item->data(Qt::UserRole).toString() == path) {
      file_list_->setCurrentItem(item);
      file_list_->scrollToItem(item);
      return;
    }
  }
}
