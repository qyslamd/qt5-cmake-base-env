#ifndef FILE_APP_DIALOGS_SETTINGS_DIALOG_H_
#define FILE_APP_DIALOGS_SETTINGS_DIALOG_H_

#include <QDialog>

// 设置占位对话框：派生自 QDialog，先放标题与占位说明，
// 具体设置项（通讯/采集/告警等）后续里程碑填充。
class SettingsDialog : public QDialog {
  Q_OBJECT
 public:
  explicit SettingsDialog(QWidget* parent = nullptr);

 private:
  void initUi();
  void initSignals();
};

#endif  // FILE_APP_DIALOGS_SETTINGS_DIALOG_H_
