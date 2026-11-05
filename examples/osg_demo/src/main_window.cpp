#include "main_window.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

#include "logger.h"
#include "osg_widget.h"
#include "scene_tree_model.h"

namespace {

constexpr int kLogDockHeightPx = 150;
constexpr int kComponentsDockWidthPx = 500;
constexpr int kLogMaxBlocks = 5000;
constexpr int kStatusMessageTimeoutMs = 3000;

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("OSG Viewer"));
  resize(1280, 800);

  // 场景与模型由构建期拷贝到可执行文件旁（见 CMakeLists 的 copy_app_assets）
  scenes_dir_ = QDir(QCoreApplication::applicationDirPath())
                    .absoluteFilePath(QStringLiteral("assets/scenes"));

  SetupUi();
  BindInteractions();
}

void MainWindow::SetupUi() {
  // OsgWidget 是普通 QWidget（内部为 QOpenGLWidget），可直接作为中心控件
  osg_widget_ = new OsgWidget(this);
  setCentralWidget(osg_widget_);

  SetupComponentsDock();
  SetupLogDock();
  SetupToolBar();

  resizeDocks({components_dock_}, {kComponentsDockWidthPx}, Qt::Horizontal);
  resizeDocks({log_dock_}, {kLogDockHeightPx}, Qt::Vertical);
}

void MainWindow::SetupComponentsDock() {
  components_dock_ = new QDockWidget(QStringLiteral("Scene Tree"), this);
  components_dock_->setObjectName(QStringLiteral("components_dock"));
  components_dock_->setAllowedAreas(Qt::LeftDockWidgetArea |
                                    Qt::RightDockWidgetArea);

  auto* panel = new QWidget(components_dock_);
  auto* layout = new QVBoxLayout(panel);

  lbl_model_ = new QLabel(QStringLiteral("No model loaded"), panel);
  lbl_model_->setWordWrap(true);
  lbl_model_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(lbl_model_);

  filter_edit_ = new QLineEdit(panel);
  filter_edit_->setPlaceholderText(QStringLiteral("Filter by name"));
  filter_edit_->setClearButtonEnabled(true);
  layout->addWidget(filter_edit_);

  // 场景树：完整 osg 层级 + 类型图标 + 可见性复选框 + 统计列
  scene_model_ = new SceneTreeModel(this);
  tree_proxy_ = new SceneTreeFilterProxy(this);
  tree_proxy_->setSourceModel(scene_model_);
  tree_proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);

  tree_view_ = new QTreeView(panel);
  tree_view_->setModel(tree_proxy_);
  tree_view_->setUniformRowHeights(true);
  tree_view_->setAnimated(false);
  // 缩进与图标占宽较大，名称列需要足够宽度，统计列固定宽度
  tree_view_->setIndentation(14);
  QHeaderView* header = tree_view_->header();
  header->setStretchLastSection(false);
  header->setSectionResizeMode(0, QHeaderView::Stretch);
  struct ColumnWidth {
    int column;
    int width;
  };
  const ColumnWidth kFixedWidths[] = {
      {SceneTreeModel::kColumnType, 74},
      {SceneTreeModel::kColumnDrawables, 64},
      {SceneTreeModel::kColumnTriangles, 72},
      {SceneTreeModel::kColumnSize, 104},
  };
  for (const ColumnWidth& spec : kFixedWidths) {
    header->setSectionResizeMode(spec.column, QHeaderView::Fixed);
    header->resizeSection(spec.column, spec.width);
  }
  layout->addWidget(tree_view_);

  components_dock_->setWidget(panel);
  addDockWidget(Qt::RightDockWidgetArea, components_dock_);
}

void MainWindow::SetupLogDock() {
  log_dock_ = new QDockWidget(QStringLiteral("Output Log"), this);
  log_dock_->setObjectName(QStringLiteral("log_dock"));
  log_dock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

  auto* container = new QWidget(log_dock_);
  auto* vbox = new QVBoxLayout(container);
  vbox->setContentsMargins(4, 4, 4, 4);
  vbox->setSpacing(4);

  auto* filter_bar = new QHBoxLayout;
  chk_show_osg_ = new QCheckBox(QStringLiteral("Show OSG logs"), container);
  chk_show_osg_->setToolTip(
      QStringLiteral("Include OSG internal messages (plugin loading, etc.)"));
  chk_show_osg_->setChecked(false);
  filter_bar->addWidget(chk_show_osg_);
  filter_bar->addStretch();
  vbox->addLayout(filter_bar);

  log_view_ = new QPlainTextEdit(container);
  log_view_->setReadOnly(true);
  log_view_->setMaximumBlockCount(kLogMaxBlocks);
  vbox->addWidget(log_view_);

  log_dock_->setWidget(container);
  addDockWidget(Qt::BottomDockWidgetArea, log_dock_);

  connect(chk_show_osg_, &QCheckBox::toggled, this,
          &MainWindow::OnShowOsgChanged);
}

void MainWindow::SetupToolBar() {
  tool_bar_ = addToolBar(QStringLiteral("Main"));
  tool_bar_->setObjectName(QStringLiteral("main_tool_bar"));
  tool_bar_->setMovable(false);

  QAction* open_action = tool_bar_->addAction(QStringLiteral("Open Model..."));
  connect(open_action, &QAction::triggered, this, &MainWindow::OpenModelDialog);

  QAction* reload_action = tool_bar_->addAction(QStringLiteral("Reload"));
  connect(reload_action, &QAction::triggered, this,
          &MainWindow::ReloadCurrentModel);

  QAction* reset_action = tool_bar_->addAction(QStringLiteral("Reset View"));
  connect(reset_action, &QAction::triggered, this, &MainWindow::ResetView);

  QAction* shot_action = tool_bar_->addAction(QStringLiteral("Screenshot..."));
  connect(shot_action, &QAction::triggered, this,
          &MainWindow::SaveScreenshotDialog);

  QAction* axes_action = tool_bar_->addAction(QStringLiteral("Axes"));
  axes_action->setCheckable(true);
  axes_action->setChecked(true);
  axes_action->setToolTip(QStringLiteral("Show or hide the world axes gizmo"));
  connect(axes_action, &QAction::toggled, this, [this](bool checked) {
    if (osg_widget_ != nullptr) {
      osg_widget_->SetAxesVisible(checked);
    }
  });

  QAction* annotations_action = tool_bar_->addAction(QStringLiteral("Annotations"));
  annotations_action->setCheckable(true);
  annotations_action->setChecked(true);
  annotations_action->setToolTip(
      QStringLiteral("Show or hide the per-part annotation balls"));
  connect(annotations_action, &QAction::toggled, this, [this](bool checked) {
    if (osg_widget_ != nullptr) {
      osg_widget_->SetAnnotationsVisible(checked);
    }
  });

  tool_bar_->addSeparator();

  // 扫描 exe 旁 assets/scenes 下的 JSON 场景，下拉条目显示 name 字段
  scene_combo_ = new QComboBox(tool_bar_);
  const QStringList json_files =
      QDir(scenes_dir_)
          .entryList(QStringList() << QStringLiteral("*.json"), QDir::Files,
                     QDir::Name);
  for (const QString& file_name : json_files) {
    const QString path = QDir(scenes_dir_).absoluteFilePath(file_name);
    QFile file(path);
    QString label = file_name;
    if (file.open(QIODevice::ReadOnly)) {
      const QString name = QJsonDocument::fromJson(file.readAll())
                               .object()
                               .value(QStringLiteral("name"))
                               .toString();
      file.close();
      if (!name.isEmpty()) {
        label = name;
      }
    }
    scene_combo_->addItem(label, path);
  }
  if (json_files.isEmpty()) {
    scene_combo_->addItem(
        QStringLiteral("(no scenes found)"), QString());
    scene_combo_->setEnabled(false);
  }
  connect(scene_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &MainWindow::OnSceneComboChanged);
  tool_bar_->addWidget(scene_combo_);

  tool_bar_->addSeparator();
  tool_bar_->addAction(components_dock_->toggleViewAction());
  tool_bar_->addAction(log_dock_->toggleViewAction());
}

void MainWindow::BindInteractions() {
  // 3D -> 树：拾取命名节点后选中对应行并同步高亮
  osg_widget_->on_node_picked_ = [this](osg::Node* node) {
    if (node == nullptr) {
      tree_view_->clearSelection();
      osg_widget_->ClearHighlight();
      return;
    }

    const QModelIndex source_index = scene_model_->IndexForNode(node);
    const QModelIndex proxy_index = tree_proxy_->mapFromSource(source_index);
    if (proxy_index.isValid()) {
      tree_view_->setCurrentIndex(proxy_index);
      tree_view_->scrollTo(proxy_index);
    }
    osg_widget_->SetHighlightNode(node);

    const QString name = QString::fromStdString(node->getName());
    statusBar()->showMessage(
        QStringLiteral("Selected: %1")
            .arg(name.isEmpty() ? QStringLiteral("(unnamed)") : name),
        kStatusMessageTimeoutMs);
  };

  // 树 -> 3D：单击选中画高亮盒，双击聚焦
  connect(tree_view_->selectionModel(), &QItemSelectionModel::currentChanged,
          this, [this](const QModelIndex& current, const QModelIndex&) {
            if (!current.isValid()) {
              osg_widget_->ClearHighlight();
              return;
            }
            osg::Node* node =
                scene_model_->NodeForIndex(tree_proxy_->mapToSource(current));
            osg_widget_->SetHighlightNode(node);
          });

  connect(tree_view_, &QTreeView::doubleClicked, this,
          [this](const QModelIndex& proxy_index) {
            osg::Node* node = scene_model_->NodeForIndex(
                tree_proxy_->mapToSource(proxy_index));
            osg_widget_->FocusOnNode(node);
          });

  // 名称过滤：先补齐全树行数据，保证未展开的节点也能命中
  connect(filter_edit_, &QLineEdit::textChanged, this,
          [this](const QString& text) {
            if (!text.isEmpty()) {
              scene_model_->FetchAll();
            }
            tree_proxy_->setFilterFixedString(text);
          });

  connect(Logger::Instance(), &Logger::MessageLogged, this,
          &MainWindow::AppendLog);
}

void MainWindow::AppendLog(const QString& msg, LogSource source) {
  log_entries_.append({msg, source});
  if (log_entries_.size() > kLogMaxBlocks) {
    log_entries_.removeFirst();
  }

  if (chk_show_osg_->isChecked() || source == LogSource::kApp) {
    log_view_->appendPlainText(msg);
    QScrollBar* bar = log_view_->verticalScrollBar();
    bar->setValue(bar->maximum());
  }
}

void MainWindow::RebuildLogView() {
  log_view_->clear();
  const bool show_osg = chk_show_osg_->isChecked();
  for (const LogEntry& e : log_entries_) {
    if (show_osg || e.source == LogSource::kApp) {
      log_view_->appendPlainText(e.text);
    }
  }
  QScrollBar* bar = log_view_->verticalScrollBar();
  bar->setValue(bar->maximum());
}

void MainWindow::OnShowOsgChanged(bool /*checked*/) { RebuildLogView(); }

void MainWindow::RefreshModelInfo(const QString& path,
                                  const QStringList& components) {
  lbl_model_->setText(QStringLiteral("Model: %1\nTotal Named Components: %2")
                          .arg(QDir::toNativeSeparators(path))
                          .arg(components.size()));
}

bool MainWindow::LoadModel(const QString& path, QString* error) {
  const bool is_json =
      path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive);
  const bool ok = is_json ? osg_widget_->LoadScene(path, error)
                          : osg_widget_->LoadModel(path, error);
  if (!ok) return false;

  current_model_path_ = QFileInfo(path).absoluteFilePath();

  // 重建场景树：根行 = 场景根，展开到零件的 Geode 层
  scene_model_->SetSceneNode(osg_widget_->GetSceneRoot());
  tree_view_->expandToDepth(1);

  const QStringList components = osg_widget_->GetModelNodeNames();
  RefreshModelInfo(current_model_path_, components);

  const int scene_idx = scene_combo_->findData(current_model_path_);
  scene_combo_->blockSignals(true);
  scene_combo_->setCurrentIndex(scene_idx);
  scene_combo_->blockSignals(false);

  qInfo() << "Model loaded:" << current_model_path_
          << "components:" << components.size();
  return true;
}

void MainWindow::OnSceneComboChanged(int index) {
  const QString path = scene_combo_->itemData(index).toString();
  if (path.isEmpty()) return;

  QString error;
  if (!LoadModel(path, &error)) {
    QMessageBox::critical(this, QStringLiteral("Scene Load Failed"), error);
    scene_combo_->blockSignals(true);
    scene_combo_->setCurrentIndex(scene_combo_->findData(current_model_path_));
    scene_combo_->blockSignals(false);
    return;
  }
  statusBar()->showMessage(QStringLiteral("Scene: %1").arg(path),
                           kStatusMessageTimeoutMs);
}

void MainWindow::OpenModelDialog() {
  const QFileInfo current(current_model_path_);
  const QString initial_dir =
      current.exists() ? current.absolutePath() : QDir::currentPath();
  const QString filter = QStringLiteral(
      "Scenes & Models (*.json *.obj *.osg *.osgt *.osgb *.ive *.glb "
      "*.gltf);;Scene Configs (*.json);;All Files (*)");

  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Open Scene / Model"), initial_dir, filter);
  if (path.isEmpty()) return;

  QString error;
  if (!LoadModel(path, &error)) {
    QMessageBox::critical(this, QStringLiteral("Load Failed"), error);
  }
}

void MainWindow::ReloadCurrentModel() {
  if (current_model_path_.isEmpty()) {
    statusBar()->showMessage(QStringLiteral("No model loaded to reload"),
                             kStatusMessageTimeoutMs);
    return;
  }

  QString error;
  if (!LoadModel(current_model_path_, &error)) {
    QMessageBox::critical(this, QStringLiteral("Reload Failed"), error);
    return;
  }
  statusBar()->showMessage(
      QStringLiteral("Reloaded: %1").arg(current_model_path_),
      kStatusMessageTimeoutMs);
}

void MainWindow::ResetView() { osg_widget_->ResetView(); }

void MainWindow::SaveScreenshotDialog() {
  const QString default_path =
      QDir::current().absoluteFilePath(QStringLiteral("screenshot.png"));
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("Save Screenshot"), default_path,
      QStringLiteral("PNG Image (*.png)"));
  if (path.isEmpty()) return;

  const QImage image = osg_widget_->GrabFramebuffer();
  if (image.isNull() || !image.save(path, "PNG")) {
    QMessageBox::warning(
        this, QStringLiteral("Save Failed"),
        QStringLiteral("Failed to save screenshot: %1").arg(path));
    return;
  }

  statusBar()->showMessage(QStringLiteral("Saved: %1").arg(path),
                           kStatusMessageTimeoutMs);
  qInfo() << "Screenshot saved:" << path
          << QString("%1x%2").arg(image.width()).arg(image.height());
}