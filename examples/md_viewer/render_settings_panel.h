#ifndef FILE_EXAMPLES_MD_VIEWER_RENDER_SETTINGS_PANEL_H_
#define FILE_EXAMPLES_MD_VIEWER_RENDER_SETTINGS_PANEL_H_

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class MarkdownView;

// 渲染设置面板：主题、行号、公式/Mermaid 开关、持久缓存、诊断。
// 直接读写 MarkdownView 的状态；控件只用 Fluent 的属性驱动
// （isCard / isSwitchButton），不写样式表
class RenderSettingsPanel : public QWidget {
  Q_OBJECT

 public:
  explicit RenderSettingsPanel(MarkdownView* view, QWidget* parent = nullptr);

 signals:
  void statusMessage(const QString& message);

 private:
  void initUi();
  void initSignals();
  void ApplyDiagnostics();
  void BrowseCacheDirectory();
  void BrowseDumpDirectory();
  void RefreshStatus();
  // 开关默认文字跟状态走（Fluent Gallery 同款写法）
  QCheckBox* CreateSwitch(bool checked);

  MarkdownView* view_ = nullptr;
  QComboBox* theme_ = nullptr;
  QCheckBox* code_line_numbers_ = nullptr;
  QCheckBox* formula_rendering_ = nullptr;
  QCheckBox* mermaid_rendering_ = nullptr;
  QCheckBox* persistent_cache_ = nullptr;
  QLineEdit* cache_directory_ = nullptr;
  QPushButton* choose_cache_ = nullptr;
  QPushButton* reset_cache_ = nullptr;
  QPushButton* clear_memory_cache_ = nullptr;
  QPushButton* clear_cache_ = nullptr;
  QCheckBox* trace_ = nullptr;
  QLineEdit* dump_directory_ = nullptr;
  QPushButton* choose_dump_ = nullptr;
  QPushButton* reset_dump_ = nullptr;
  QLabel* status_highlight_ = nullptr;
  QLabel* status_formula_ = nullptr;
  QLabel* status_mermaid_ = nullptr;
  QPushButton* refresh_status_ = nullptr;
  QPushButton* reset_stats_ = nullptr;
};

#endif  // FILE_EXAMPLES_MD_VIEWER_RENDER_SETTINGS_PANEL_H_
