#ifndef FILE_EXAMPLES_MD_VIEWER_MD_VIEWER_WINDOW_H_
#define FILE_EXAMPLES_MD_VIEWER_MD_VIEWER_WINDOW_H_

#include <QMainWindow>
#include <QString>

class QAction;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QTimer;
enum class MarkdownTheme;
class FluentWindowFrame;
class MarkdownOutlineWidget;
class MarkdownView;
class RenderSettingsPanel;
class RenderStatusPanel;

// 独立的 Markdown 查看器：单篇与"整个目录"两种打开方式，目录铺在左侧文件
// 列表里。渲染全交给 src/markdown 的 MarkdownView——本类只管宿主策略：
// 窗口壳、菜单、侧栏、状态栏。
class MdViewerWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MdViewerWindow(QWidget* parent = nullptr);

  // 命令行参数进来：目录就铺开侧栏并看第一篇，文件直接看
  void OpenPath(const QString& path);

 private:
  void createActions();
  void initUi();
  void initSignals();

  void OpenFileDialog();
  void OpenDirectoryDialog();
  // Fluent 壳与文档主题一起切
  void ApplyTheme(MarkdownTheme theme);
  // 标题栏主题按钮：浅色/暗色之间翻转
  void ToggleTheme();
  // Windows：DWM 窗口模糊开关（调色板透明度与 backdrop 一起切）
  void SetWindowBlur(bool enabled);
  void FocusSearch();
  // incremental=true 是"边打字边找"：从当前命中处继续匹配，不清空重来
  void FindNext(bool backward, bool incremental = false);
  void UpdateSearchActionIcons();
  void ExportPdf();
  // 只填列表，不加载——加载由 ShowFile 负责，免得两边互相触发
  void RefreshDirectoryListing(const QString& dir);
  void ShowFile(const QString& path);
  void SelectInList(const QString& path);

  // 菜单里挂着；菜单栏在 fluent 壳下会被搬进标题栏
  QAction* open_file_action_ = nullptr;
  QAction* open_dir_action_ = nullptr;
  QAction* quit_action_ = nullptr;
  QAction* about_action_ = nullptr;
  QAction* export_pdf_action_ = nullptr;
  QAction* find_action_ = nullptr;
  QAction* find_next_action_ = nullptr;
  QAction* find_prev_action_ = nullptr;
  // 搜索框内的模式开关（挂在 Fluent 标题栏的 QLineEdit 上）
  QAction* case_action_ = nullptr;
  QAction* word_action_ = nullptr;
  // Fluent 标题栏自带的搜索框；控件归标题栏所有
  QLineEdit* search_edit_ = nullptr;
  // 输入防抖：连续打字只搜最后一档
  QTimer* search_timer_ = nullptr;

  MarkdownView* view_ = nullptr;
  MarkdownOutlineWidget* outline_ = nullptr;
  RenderSettingsPanel* render_settings_ = nullptr;
  RenderStatusPanel* render_status_ = nullptr;
  QListWidget* file_list_ = nullptr;
  QLabel* path_label_ = nullptr;
  // 列表当前列的是哪个目录（ShowFile 据此决定要不要重扫）
  QString listed_dir_;

  // 无边框窗口壳（QWK + Fluent 标题栏）
  FluentWindowFrame* fluent_frame_ = nullptr;
};

#endif  // FILE_EXAMPLES_MD_VIEWER_MD_VIEWER_WINDOW_H_
