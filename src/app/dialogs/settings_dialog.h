#ifndef FILE_APP_DIALOGS_SETTINGS_DIALOG_H_
#define FILE_APP_DIALOGS_SETTINGS_DIALOG_H_

#include <QDialog>
#include <QStringList>

class QComboBox;
class QHBoxLayout;
class QListWidget;
class QSpinBox;
class QStackedWidget;
class QVBoxLayout;
class SwitchButton;

// 设置对话框：左侧分类导航 + 右侧分页（QStackedWidget），按功能分组。
// 控件变更即时写入 ConfigManager，不设确定/取消；日志相关项由 Logger
// 监听 configChanged 即时生效，文件轮转参数下次启动生效。
// 新增分组：写一个 createXxxPage() 并经 addCategoryPage 注册即可。
class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(QWidget* parent = nullptr);

 private:
  void initUi();
  void initSignals();

  // 注册一个分组页：追加到右侧分页栈并同步左栏条目
  void addCategoryPage(const QString& name, QWidget* page);

  // 各功能分组页
  QWidget* createLogPage();
  QWidget* createLogFormatPage();

  // 页面标题（大标题 + 副标题）
  void addPageHeader(QVBoxLayout* layout, const QString& title,
                     const QString& subtitle);
  // 卡片容器：一组相关设置行
  QWidget* createSettingsCard(QWidget* parent, const QString& title);
  // 设置行：左侧标题+描述，返回右侧控件区布局
  QHBoxLayout* addSettingRow(QWidget* parent, const QString& title,
                             const QString& description);

  // 以下三个在设置行内创建控件并双向绑定配置，返回控件以便追加设置
  // （如 setSuffix）。scale 用于「显示值 × scale = 配置值」的换算，
  // 例如配置以字节存储而界面以 MB 呈现
  QSpinBox* addSpinBoxRow(QWidget* parent, const QString& title,
                          const QString& description, const QString& config_key,
                          int min, int max, int default_value, int scale = 1);
  QComboBox* addComboBoxRow(QWidget* parent, const QString& title,
                            const QString& description,
                            const QString& config_key,
                            const QStringList& items, int default_index);
  SwitchButton* addCheckBoxRow(QWidget* parent, const QString& title,
                               const QString& description,
                               const QString& config_key, bool default_value);

  QListWidget* category_list_ = nullptr;
  QStackedWidget* page_stack_ = nullptr;
};

#endif  // FILE_APP_DIALOGS_SETTINGS_DIALOG_H_
