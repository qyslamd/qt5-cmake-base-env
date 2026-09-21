#include "testing_support.h"

#include <pqTestUtility.h>

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>

#include "qttesting_xml.h"

UiTestSupport::UiTestSupport(QObject* parent)
    : QObject(parent), utility_(new pqTestUtility(this)) {
  utility_->addEventObserver("xml", new AppQtTestingXMLEventObserver(utility_));
  utility_->addEventSource("xml", new AppQtTestingXMLEventSource(utility_));
}

void UiTestSupport::attachMenu(QMainWindow* window) {
  QMenu* menu = window->menuBar()->addMenu(QStringLiteral("测试(&C)"));

  QAction* record_action = menu->addAction(QStringLiteral("录制事件(&R)..."));
  connect(record_action, &QAction::triggered, this, [this, window]() {
    const QString file = QFileDialog::getSaveFileName(
        window, QStringLiteral("测试脚本另存为"), QString(),
        QStringLiteral("XML 文件 (*.xml)"));
    if (file.isEmpty()) {
      return;
    }
    // 录制靠事件过滤器捕获投递到窗口的事件，窗口必须处于活动状态
    QApplication::setActiveWindow(window);
    utility_->recordTests(file);
  });

  QAction* play_action = menu->addAction(QStringLiteral("回放事件(&P)..."));
  connect(play_action, &QAction::triggered, this, [this, window]() {
    const QString file = QFileDialog::getOpenFileName(
        window, QStringLiteral("选择测试脚本"), QString(),
        QStringLiteral("XML 文件 (*.xml)"));
    if (file.isEmpty()) {
      return;
    }
    // 与录制端对称：回放同样要求窗口处于活动状态，否则事件投不到它
    QApplication::setActiveWindow(window);
    playFile(file);
  });
}

bool UiTestSupport::playFile(const QString& file) {
  return utility_->playTests(file);
}
