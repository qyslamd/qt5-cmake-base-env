#ifndef FILE_APP_DIALOGS_CRASH_REPORT_DIALOG_H_
#define FILE_APP_DIALOGS_CRASH_REPORT_DIALOG_H_

#include <QDialog>
#include <QString>
#include <QVector>

#include "crashhandler/crash_bootstrap.h"

class QPushButton;

// 崩溃报告的默认导出目录：桌面 → 文档 → 主目录，其下新建
// CrashReport-<yyyyMMdd-HHmmss>/。命令行导出（--export-report）也用同一处
QString DefaultCrashReportDir();

// 上次运行发生过崩溃时，提示用户把报告导出后交给开发者。
// 崩溃那一刻进程正在死（Crashpad 模式下没有弹窗机会），所以这件事只能在
// 下次启动做
class CrashReportDialog : public QDialog {
  Q_OBJECT

 public:
  explicit CrashReportDialog(const QVector<crash::PendingReport>& reports,
                             QWidget* parent = nullptr);

 private:
  void initUi();
  void initSignals();

  // 导出到默认目录，并把结果告诉用户（成功则给出路径与「打开文件夹」）
  void onExportClicked();

  QVector<crash::PendingReport> reports_;
  QPushButton* export_button_ = nullptr;
  QPushButton* later_button_ = nullptr;
};

#endif  // FILE_APP_DIALOGS_CRASH_REPORT_DIALOG_H_
