#include "crash_report_dialog.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>

#include "logger/logger.h"

void PromptPendingCrashReportIfAny() {
  if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
    LOG_INFO("MAIN",
             "无桌面环境，跳过崩溃报告提示（可用 --export-report 导出）");
    return;
  }

  const QVector<crash::PendingReport> reports =
      crash::ListPendingReports(crash::DatabaseDir());
  if (reports.isEmpty()) {
    return;
  }
  LOG_INFO("MAIN", "检测到 {} 份待导出的崩溃报告", reports.size());

  CrashReportDialog dialog(reports);
  dialog.exec();
}

CrashReportDialog::CrashReportDialog(
    const QVector<crash::PendingReport>& reports, QWidget* parent)
    : QDialog(parent), reports_(reports) {
  initUi();
  initSignals();
}

void CrashReportDialog::initUi() {
  setObjectName(QStringLiteral("crashReportDialog"));
  setWindowTitle(QStringLiteral("崩溃报告"));
  setMinimumWidth(440);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 20, 24, 16);
  layout->setSpacing(12);

  auto* title = new QLabel(QStringLiteral("上次运行发生过崩溃"), this);
  title->setObjectName(QStringLiteral("crashReportTitle"));
  layout->addWidget(title);

  // 最近一次的崩溃时间：客户描述"什么时候崩的"时能对上号
  qint64 newest = 0;
  for (const crash::PendingReport& report : reports_) {
    newest = qMax(newest, report.created_at);
  }

  auto* detail = new QLabel(
      QStringLiteral("已生成 %1 份崩溃报告，需要发送给开发者才能定位问题。\n"
                     "最近一次崩溃：%2")
          .arg(reports_.size())
          .arg(QDateTime::fromSecsSinceEpoch(newest).toString(
              QStringLiteral("yyyy-MM-dd HH:mm:ss"))),
      this);
  detail->setObjectName(QStringLiteral("crashReportDetail"));
  detail->setWordWrap(true);
  layout->addWidget(detail);

  auto* buttons = new QHBoxLayout();
  buttons->addStretch();

  later_button_ = new QPushButton(QStringLiteral("稍后"), this);
  later_button_->setObjectName(QStringLiteral("crashReportLaterButton"));
  later_button_->setProperty("role", "secondary");
  buttons->addWidget(later_button_);

  export_button_ = new QPushButton(QStringLiteral("导出报告"), this);
  export_button_->setObjectName(QStringLiteral("crashReportExportButton"));
  export_button_->setProperty("role", "primary");
  export_button_->setDefault(true);
  buttons->addWidget(export_button_);

  layout->addLayout(buttons);
}

void CrashReportDialog::initSignals() {
  connect(later_button_, &QPushButton::clicked, this, &QDialog::reject);
  connect(export_button_, &QPushButton::clicked, this,
          &CrashReportDialog::onExportClicked);
}

void CrashReportDialog::onExportClicked() {
  // 导出到默认目录，与 --export-report 用的是同一处（crash::DefaultExportDir）
  const crash::ExportResult result = crash::ExportTo(QString());

  if (!result.ok() || result.exported == 0) {
    QMessageBox::warning(this, QStringLiteral("导出失败"),
                         result.error.isEmpty()
                             ? QStringLiteral("没有找到可导出的崩溃报告。")
                             : result.error);
    return;
  }

  // 成功：给出路径，并让用户能一键打开确认文件确实在
  QMessageBox box(this);
  box.setIcon(QMessageBox::Information);
  box.setWindowTitle(QStringLiteral("导出完成"));
  box.setText(QStringLiteral("已导出 %1 份崩溃报告。").arg(result.exported));
  box.setInformativeText(
      QStringLiteral("位置：%1\n\n请把该文件夹发送给开发者，发送前不要删除它。")
          .arg(QDir::toNativeSeparators(result.target_dir)));
  QPushButton* open_button =
      box.addButton(QStringLiteral("打开文件夹"), QMessageBox::ActionRole);
  box.addButton(QStringLiteral("关闭"), QMessageBox::RejectRole);
  box.exec();

  if (box.clickedButton() == open_button) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(result.target_dir));
  }
  accept();
}
