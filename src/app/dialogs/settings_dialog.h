#ifndef FILE_APP_DIALOGS_SETTINGS_DIALOG_H_
#define FILE_APP_DIALOGS_SETTINGS_DIALOG_H_

#include <QDialog>

class QComboBox;
class QSpinBox;

// 设置对话框：示例性质的少量设置项，全部读写 ConfigManager。
// 模板使用者可在此基础上增删设置组，无需改动其他模块。
class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(QWidget* parent = nullptr);

 private:
  void initUi();
  void initSignals();
  // 从 ConfigManager 读取当前值填充控件
  void loadFromConfig();
  // 把控件值写回 ConfigManager 并落盘
  void saveToConfig();

 private slots:
  void onAccepted();

 private:
  QComboBox* log_level_box_ = nullptr;
  QSpinBox* keep_days_box_ = nullptr;
  QSpinBox* max_file_size_box_ = nullptr;
  QSpinBox* max_file_count_box_ = nullptr;
};

#endif  // FILE_APP_DIALOGS_SETTINGS_DIALOG_H_
