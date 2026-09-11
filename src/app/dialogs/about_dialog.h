#ifndef FILE_APP_DIALOGS_ABOUT_DIALOG_H_
#define FILE_APP_DIALOGS_ABOUT_DIALOG_H_

#include <QDialog>

// 关于对话框：展示应用名、版本号、构建时间与 Qt 版本
class AboutDialog : public QDialog {
  Q_OBJECT

 public:
  explicit AboutDialog(QWidget* parent = nullptr);

 private:
  void initUi();
  void initSignals();
};

#endif  // FILE_APP_DIALOGS_ABOUT_DIALOG_H_
