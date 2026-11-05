#ifndef OSG_QT_VIEWER_SRC_MAIN_WINDOW_H_
#define OSG_QT_VIEWER_SRC_MAIN_WINDOW_H_

#include <QMainWindow>
#include <QString>
#include <QVector>

#include "logger.h"

class QCheckBox;
class QComboBox;
class QDockWidget;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QToolBar;
class QTreeView;
class OsgWidget;
class SceneTreeFilterProxy;
class SceneTreeModel;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override = default;

  bool LoadModel(const QString& path, QString* error = nullptr);
  OsgWidget* GetOsgWidget() const { return osg_widget_; }

 private slots:
  void OpenModelDialog();
  void ReloadCurrentModel();
  void ResetView();
  void SaveScreenshotDialog();
  void OnSceneComboChanged(int index);
  void OnShowOsgChanged(bool checked);
  void AppendLog(const QString& msg, LogSource source);

 private:
  void SetupUi();
  void SetupToolBar();
  void SetupComponentsDock();
  void SetupLogDock();
  void BindInteractions();
  void RefreshModelInfo(const QString& path, const QStringList& components);
  void RebuildLogView();

  struct LogEntry {
    QString text;
    LogSource source;
  };

  QVector<LogEntry> log_entries_;

  OsgWidget* osg_widget_ = nullptr;
  QToolBar* tool_bar_ = nullptr;
  QDockWidget* components_dock_ = nullptr;
  QDockWidget* log_dock_ = nullptr;
  QLineEdit* filter_edit_ = nullptr;
  QTreeView* tree_view_ = nullptr;
  SceneTreeModel* scene_model_ = nullptr;
  SceneTreeFilterProxy* tree_proxy_ = nullptr;
  QLabel* lbl_model_ = nullptr;
  QPlainTextEdit* log_view_ = nullptr;
  QCheckBox* chk_show_osg_ = nullptr;
  QComboBox* scene_combo_ = nullptr;

  QString current_model_path_;
  QString scenes_dir_;
};

#endif  // OSG_QT_VIEWER_SRC_MAIN_WINDOW_H_
