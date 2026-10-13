#ifndef FILE_APP_DIALOGS_CRASH_REPORT_DIALOG_H_
#define FILE_APP_DIALOGS_CRASH_REPORT_DIALOG_H_

#include <QDialog>
#include <QString>
#include <QVector>

#include "crashhandler/crash.h"

class QPushButton;

// 有待处理的崩溃报告就弹对话框提示导出；没有报告、或无桌面环境时静默返回
// （模态框在无桌面环境里没人点，会一直挂着）。"什么时候调"仍由调用方决定
void PromptPendingCrashReportIfAny();

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
