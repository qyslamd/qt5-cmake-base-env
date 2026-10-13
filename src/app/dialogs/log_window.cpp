#include "log_window.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include "widgets/log_output_panel.h"

LogWindow::LogWindow(QWidget* parent) : QDialog(parent) {
  initUi();
  initSignals();
}

void LogWindow::initUi() {
  setObjectName(QStringLiteral("logWindow"));
  setWindowTitle(QStringLiteral("日志"));
  resize(960, 560);
  setSizeGripEnabled(true);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // LogOutputPanel 自行挂接 QtConsoleSink 与历史缓冲，此处无需额外接线
  panel_ = new LogOutputPanel(this);
  layout->addWidget(panel_);

  // 应用内的关闭入口。**别省**：UI 回放录不到标题栏的 X——那是非客户区，
  // 只发 QCloseEvent，没有鼠标事件经过录制器的过滤器，回放时窗口会一直挂着。
  // 凡是被回放打开的窗口都得有这么一个按钮，否则它在脚本里"只进不出"。
  // 与设置对话框的 settingsCloseButton 同一做法
  auto* button_row = new QHBoxLayout();
  button_row->setContentsMargins(16, 8, 16, 12);
  button_row->addStretch();
  close_btn_ = new QPushButton(QStringLiteral("关闭"), this);
  close_btn_->setObjectName(QStringLiteral("logCloseButton"));
  button_row->addWidget(close_btn_);
  layout->addLayout(button_row);
}

void LogWindow::initSignals() {
  connect(close_btn_, &QPushButton::clicked, this, &QDialog::close);
}
