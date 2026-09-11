#include "log_window.h"

#include <QVBoxLayout>

#include "log_output_panel.h"

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
}

void LogWindow::initSignals() {}
