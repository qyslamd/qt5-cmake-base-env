#include "md_viewer_window.h"

#include <fluenttitlebar.h>
#include <fluentui3style.h>
#include <fluentwindowframe.h>

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileInfoList>
#include <QFont>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPrinter>
#include <QShortcut>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "markdown_outline_widget.h"
#include "markdown_view.h"
#include "render_settings_panel.h"
#include "render_status_panel.h"

namespace {

#ifdef Q_OS_WIN
// 改 Qt::WindowStaysOnTopHint 会重建 HWND，QWK 的模糊等窗口属性得重来；
// 置顶用 SetWindowPos 即可（Gallery 同款）
void SetTopMost(QWidget* window, bool top_most) {
  SetWindowPos(reinterpret_cast<HWND>(window->winId()),
               top_most ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}
#else
void SetTopMost(QWidget* window, bool top_most) {
  window->setWindowFlag(Qt::WindowStaysOnTopHint, top_most);
  window->show();
}
#endif

// 与模块的 IsMarkdownFile 一个口径：目录里认这两种后缀
const QStringList& MarkdownFilters() {
  static const QStringList kFilters{QStringLiteral("*.md"),
                                    QStringLiteral("*.markdown")};
  return kFilters;
}

// 没有图标资源，就把字母/箭头画进 QPixmap：激活态用高亮色加粗
QIcon TextActionIcon(const QString& text, bool active) {
  constexpr int kIconSize = 18;
  QPixmap pixmap(kIconSize, kIconSize);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  QFont font = QApplication::font();
  font.setPixelSize(12);
  font.setBold(active);
  painter.setFont(font);
  painter.setPen(active ? QApplication::palette().color(QPalette::Highlight)
                        : QApplication::palette().color(QPalette::WindowText));
  painter.drawText(pixmap.rect(), Qt::AlignCenter, text);
  return QIcon(pixmap);
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
  export_pdf_action_ = new QAction(QStringLiteral("导出 PDF…"), this);

  // 查找动作不建菜单：快捷键挂在窗口上，按钮内嵌进标题栏搜索框
  find_action_ = new QAction(QStringLiteral("查找…"), this);
  find_action_->setShortcut(QKeySequence::Find);
  find_next_action_ = new QAction(QStringLiteral("下一个"), this);
  find_next_action_->setShortcut(QKeySequence(Qt::Key_F3));
  find_next_action_->setToolTip(QStringLiteral("查找下一个（F3）"));
  find_prev_action_ = new QAction(QStringLiteral("上一个"), this);
  find_prev_action_->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
  find_prev_action_->setToolTip(QStringLiteral("查找上一个（Shift+F3）"));
  case_action_ = new QAction(QStringLiteral("区分大小写"), this);
  case_action_->setCheckable(true);
  case_action_->setToolTip(QStringLiteral("区分大小写"));
  word_action_ = new QAction(QStringLiteral("全词匹配"), this);
  word_action_->setCheckable(true);
  word_action_->setToolTip(QStringLiteral("全词匹配"));
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
  file_menu->addAction(export_pdf_action_);
  file_menu->addSeparator();
  file_menu->addAction(quit_action_);

  QMenu* view_menu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
  QMenu* help_menu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
  help_menu->addAction(about_action_);

  // 没有菜单承载也要让快捷键生效：动作挂到窗口上
  addAction(find_action_);
  addAction(find_next_action_);
  addAction(find_prev_action_);

  // 目录里扫到的 Markdown 铺在这：显示文件名，UserRole 存全路径
  auto* dock = new QDockWidget(QStringLiteral("文档"), this);
  dock->setObjectName(QStringLiteral("documentDock"));
  file_list_ = new QListWidget(dock);
  file_list_->setAlternatingRowColors(true);
  dock->setWidget(file_list_);
  addDockWidget(Qt::LeftDockWidgetArea, dock);
  view_menu->addAction(dock->toggleViewAction());

  // 大纲：标题层级 + 点击回跳；当前章节由本类按滚动位置反查
  auto* outline_dock = new QDockWidget(QStringLiteral("大纲"), this);
  outline_dock->setObjectName(QStringLiteral("outlineDock"));
  outline_ = new MarkdownOutlineWidget(outline_dock);
  outline_dock->setWidget(outline_);
  addDockWidget(Qt::LeftDockWidgetArea, outline_dock);
  view_menu->addAction(outline_dock->toggleViewAction());

  // 渲染状态：引擎加载情况与渲染/命中计数，独立成 dock 便于常开观察
  auto* status_dock = new QDockWidget(QStringLiteral("渲染状态"), this);
  status_dock->setObjectName(QStringLiteral("renderStatusDock"));
  render_status_ = new RenderStatusPanel(view_, status_dock);
  status_dock->setWidget(render_status_);
  addDockWidget(Qt::RightDockWidgetArea, status_dock);
  view_menu->addAction(status_dock->toggleViewAction());

  // 渲染设置：主题、行号、渲染开关、持久缓存、诊断
  auto* settings_dock = new QDockWidget(QStringLiteral("渲染设置"), this);
  settings_dock->setObjectName(QStringLiteral("renderSettingsDock"));
  settings_dock->setMinimumWidth(280);
  render_settings_ = new RenderSettingsPanel(view_, settings_dock);
  settings_dock->setWidget(render_settings_);
  addDockWidget(Qt::RightDockWidgetArea, settings_dock);
  view_menu->addAction(settings_dock->toggleViewAction());

  path_label_ = new QLabel(this);
  statusBar()->addPermanentWidget(path_label_);

  // 无边框壳 + Fluent 标题栏；菜单栏挂进标题栏（官方 Gallery 与 demo_app 同款）
  fluent_frame_ = new FluentWindowFrame(this, this);
  fluent_frame_->installChromeHeader(menuBar());
  // 强调色按钮只在标题栏内部改选中态并发信号，查看器没有消费方，隐藏
  for (QToolButton* button : fluent_frame_->titleBar()->accentButtons()) {
    button->setVisible(false);
  }
  // 与文档主题初始一致；标题栏据此选搜索图标等自身角色的配色
  fluent_frame_->titleBar()->setThemeDark(view_->theme() !=
                                          MarkdownTheme::Light);
#ifdef Q_OS_WIN
  // DWM 在窗口后面做模糊（Win8+ 走 SetWindowCompositionAttribute）。
  // QWK 在拿不到 HWND 时会缓存属性，show 后自动补应用
  fluent_frame_->setWindowBackdrop(QStringLiteral("dwm-blur"));
#endif

  // 直接用标题栏自带的搜索框：模式开关与上一个/下一个内嵌成 Trailing action
  search_edit_ = fluent_frame_->titleBar()->searchLineEdit();
  search_edit_->setObjectName(QStringLiteral("titleSearchEdit"));
  search_edit_->addAction(case_action_, QLineEdit::TrailingPosition);
  search_edit_->addAction(word_action_, QLineEdit::TrailingPosition);
  // 上一个/下一个不内嵌到搜索框：它们的 QAction 仍挂在窗口上，F3/Shift+F3
  // 照常可用；输入防抖后手动跳转的需求也少了
  search_timer_ = new QTimer(this);
  search_timer_->setSingleShot(true);
  search_timer_->setInterval(250);
  UpdateSearchActionIcons();
}

void MdViewerWindow::initSignals() {
  // 标题栏自带的主题按钮：Gallery 同款，点了就整窗（壳 + 文档）换肤
  connect(fluent_frame_->titleBar()->themeButton(), &QToolButton::clicked, this,
          &MdViewerWindow::ToggleTheme);
  connect(fluent_frame_->titleBar()->pinButton(), &QToolButton::toggled, this,
          [this](bool checked) { SetTopMost(this, checked); });
  connect(render_settings_, &RenderSettingsPanel::themeChanged, this,
          &MdViewerWindow::ApplyTheme);
  connect(render_settings_, &RenderSettingsPanel::windowBlurEnabledChanged,
          this, &MdViewerWindow::SetWindowBlur);
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
  connect(view_, &MarkdownView::outlineChanged, this,
          [this] { outline_->SetHeadings(view_->outline()); });
  connect(outline_, &MarkdownOutlineWidget::headingActivated, view_,
          &MarkdownView::ScrollToAnchor);
  connect(render_settings_, &RenderSettingsPanel::statusMessage, this,
          [this](const QString& message) {
            statusBar()->showMessage(message, 5000);
          });
  connect(render_status_, &RenderStatusPanel::statusMessage, this,
          [this](const QString& message) {
            statusBar()->showMessage(message, 5000);
          });
  // 当前章节由宿主反查：最后一个 AnchorY <= 滚动位置 的标题。
  // 标题自带 margin，顶部标题的 y 通常大于 0；滚动到顶时留一段容差，
  // 否则第一项永远选不中
  connect(view_, &MarkdownView::viewportScrolled, this, [this](int y) {
    constexpr int kAnchorSlackPx = 32;
    QString current;
    for (const MarkdownHeading& heading : view_->outline()) {
      const std::optional<int> anchor_y = view_->AnchorY(heading.anchor);
      if (!anchor_y.has_value()) {
        continue;
      }
      if (*anchor_y > y + kAnchorSlackPx) {
        break;
      }
      current = heading.anchor;
    }
    outline_->SetCurrentAnchor(current);
  });

  connect(view_, &MarkdownView::loadFailed, this,
          [this](const QString& path, const QString& reason) {
            statusBar()->showMessage(
                QStringLiteral("打开失败：%1（%2）")
                    .arg(QFileInfo(path).fileName(), reason),
                5000);
          });

  connect(export_pdf_action_, &QAction::triggered, this,
          &MdViewerWindow::ExportPdf);
  // 链接策略（与 demo_app 的 MarkdownWindow 同一套，宿主各自持有）：
  // 页内锚点只滚；本地 .md 换篇（带锚点就载入后滚过去）；其余交系统。
  // 锚点判定要比对"去掉 fragment 是不是当前文档的目录"——qlitehtml 把纯
  // fragment 拼到基准 URL（目录）上，判 isLocalFile/路径空永远判不中
  connect(view_, &MarkdownView::linkClicked, this, [this](const QUrl& url) {
    const QString fragment = url.fragment();
    if (!fragment.isEmpty()) {
      const QUrl no_frag = url.adjusted(QUrl::RemoveFragment);
      const QUrl here =
          view_->has_document()
              ? QUrl::fromLocalFile(
                    QFileInfo(view_->current_file()).absolutePath() +
                    QLatin1Char('/'))
              : QUrl();
      if (no_frag.isEmpty() || no_frag == here) {
        view_->ScrollToAnchor(fragment);
        return;
      }
    }
    const QString path = url.isLocalFile() ? url.toLocalFile() : QString();
    if (!path.isEmpty() &&
        (path.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive) ||
         path.endsWith(QStringLiteral(".markdown"), Qt::CaseInsensitive))) {
      ShowFile(path);
      view_->ScrollToAnchor(fragment);
      return;
    }
    const bool web_link = url.scheme() == QLatin1String("http") ||
                          url.scheme() == QLatin1String("https") ||
                          url.scheme() == QLatin1String("mailto");
    if (web_link || QFileInfo(path).isFile()) {
      QDesktopServices::openUrl(url);
    }
  });
  connect(find_action_, &QAction::triggered, this,
          &MdViewerWindow::FocusSearch);
  connect(find_next_action_, &QAction::triggered, this,
          [this] { FindNext(/*backward=*/false); });
  connect(find_prev_action_, &QAction::triggered, this,
          [this] { FindNext(/*backward=*/true); });
  connect(case_action_, &QAction::toggled, this, [this] {
    UpdateSearchActionIcons();
    search_timer_->stop();
    if (!search_edit_->text().isEmpty()) {
      FindNext(/*backward=*/false);
    }
  });
  connect(word_action_, &QAction::toggled, this, [this] {
    UpdateSearchActionIcons();
    search_timer_->stop();
    if (!search_edit_->text().isEmpty()) {
      FindNext(/*backward=*/false);
    }
  });
  // 边打字边找：防抖，停下来 250ms 才搜；incremental 保证换关键词时
  // 从当前命中处继续，而不是跳到下一个
  connect(search_edit_, &QLineEdit::textChanged, this,
          [this](const QString& text) {
            if (text.isEmpty()) {
              search_timer_->stop();
              return;
            }
            search_timer_->start();
          });
  connect(search_timer_, &QTimer::timeout, this,
          [this] { FindNext(/*backward=*/false, /*incremental=*/true); });
  // 回车 = 立即下一个，并取消挂起的自动搜索
  connect(search_edit_, &QLineEdit::returnPressed, this, [this] {
    search_timer_->stop();
    FindNext(/*backward=*/false);
  });
  // Esc 把焦点还给正文；搜索框与它的 action 保持常驻，不再有"关闭查找栏"
  auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  connect(escape, &QShortcut::activated, this, [this] {
    if (search_edit_->hasFocus()) {
      view_->setFocus();
    }
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
      this, QStringLiteral("打开 Markdown"), start_dir,
      QStringLiteral("Markdown 文档 (*.md *.markdown);;所有文件 (*)"));
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

void MdViewerWindow::ApplyTheme(MarkdownTheme theme) {
  const bool dark = theme == MarkdownTheme::Dark;
  qApp->setProperty("_q_colorscheme", dark ? 1 : 0);
  // 重建样式，让 FluentUI3Style 按新的配色方案重算调色板（Gallery 同款）
  qApp->setStyle(new FluentUI3Style);
  fluent_frame_->titleBar()->setThemeDark(dark);
  // 搜索框里的 A/T/箭头是运行时按调色板画的，换肤后要重画
  UpdateSearchActionIcons();
  view_->SetTheme(theme);
  // 面板是被动同步方，用 QSignalBlocker 防止再发一次 themeChanged
  render_settings_->SetTheme(theme);
}

void MdViewerWindow::ToggleTheme() {
  const bool dark = qApp->property("_q_colorscheme").toInt() == 1;
  ApplyTheme(dark ? MarkdownTheme::Light : MarkdownTheme::Dark);
}

void MdViewerWindow::SetWindowBlur(bool enabled) {
#ifdef Q_OS_WIN
  // 只切 backdrop 不够：关掉 blur 后调色板若仍透明，窗口会露底。
  // _q_widget_mode 与样式要一起切
  qApp->setProperty("_q_widget_mode", enabled ? 2 : 0);
  qApp->setStyle(new FluentUI3Style);
  if (enabled) {
    fluent_frame_->setWindowBackdrop(QStringLiteral("dwm-blur"));
  } else {
    fluent_frame_->clearWindowBackdrop();
  }
#else
  Q_UNUSED(enabled);
#endif
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
  setWindowTitle(QStringLiteral("Markdown 查看器 - %1").arg(info.fileName()));
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

void MdViewerWindow::FocusSearch() {
  search_edit_->setFocus();
  search_edit_->selectAll();  // 再按 Ctrl+F 是"改查找词"，不是接着找
}

void MdViewerWindow::UpdateSearchActionIcons() {
  case_action_->setIcon(
      TextActionIcon(QStringLiteral("A"), case_action_->isChecked()));
  word_action_->setIcon(
      TextActionIcon(QStringLiteral("T"), word_action_->isChecked()));
  find_prev_action_->setIcon(TextActionIcon(QStringLiteral("↑"), false));
  find_next_action_->setIcon(TextActionIcon(QStringLiteral("↓"), false));
}

void MdViewerWindow::FindNext(bool backward, bool incremental) {
  const QString needle = search_edit_->text();
  if (needle.isEmpty()) {
    return;
  }
  bool wrapped = false;
  MarkdownView::FindFlags flags =
      backward ? MarkdownView::FindBackward : MarkdownView::FindNoFlag;
  if (case_action_->isChecked()) {
    flags |= MarkdownView::FindCaseSensitively;
  }
  if (word_action_->isChecked()) {
    flags |= MarkdownView::FindWholeWords;
  }
  if (!view_->FindText(needle, flags, &wrapped, incremental)) {
    statusBar()->showMessage(QStringLiteral("未找到：%1").arg(needle), 3000);
    return;
  }
  // 绕回说明找了一圈——提示一下，否则用户以为"没动"
  if (wrapped) {
    statusBar()->showMessage(
        backward ? QStringLiteral("已绕回末尾") : QStringLiteral("已绕回开头"),
        3000);
  }
}

void MdViewerWindow::ExportPdf() {
  if (!view_->has_document()) {
    statusBar()->showMessage(QStringLiteral("先打开一篇 Markdown 再导出"),
                             3000);
    return;
  }
  QString default_path;
  if (!view_->current_file().isEmpty()) {
    const QFileInfo info(view_->current_file());
    default_path = info.absolutePath() + QLatin1Char('/') +
                   info.completeBaseName() + QStringLiteral(".pdf");
  } else {
    default_path = QDir::homePath() + QStringLiteral("/untitled.pdf");
  }

  QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出 PDF"), default_path,
      QStringLiteral("PDF 文件 (*.pdf)"));
  if (path.isEmpty()) {
    return;  // 用户取消
  }
  if (!path.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
    path += QStringLiteral(".pdf");
  }

  // 分页与 print 媒体重排都在控件里（见 MarkdownView::Print 的注释）
  QPrinter printer(QPrinter::HighResolution);
  printer.setOutputFormat(QPrinter::PdfFormat);
  printer.setOutputFileName(path);
  view_->Print(&printer);

  statusBar()->showMessage(
      QStringLiteral("已导出：%1").arg(QFileInfo(path).fileName()), 5000);
}
