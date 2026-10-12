#include "testing_support.h"

#include <pqTestUtility.h>

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QTimer>

#include "qttesting_xml.h"
#include "tour_menu.h"  // HostMenuBar

// 静态库里的 qrc 符号没人引用就不会被链进来，得有代码显式引用一次。
//
// 这个函数**必须留在文件作用域**，不能放进匿名命名空间、也不能放进类成员函数：
// Q_INIT_RESOURCE 展开后是一个块作用域的 extern 声明，它会把
// qInitResources_QtTesting 声明到"最近的封闭命名空间"里——放进匿名命名空间就变成
// `anonymous namespace'::qInitResources_QtTesting，而 rcc
// 生成的那个在全局作用域， 于是链接期报 LNK2019（这条注释是踩过之后补的）
//
// 少了它会怎样：pqRecordEventsDialog / pqPlayBackEventsDialog 的按钮全靠
// QtTesting.qrc 里的图标，而那个「校验」按钮**只有图标、没有文字**——资源缺失时
// 面板上是一排空白按钮，看着像没有功能
static void InitQtTestingResources() { Q_INIT_RESOURCE(QtTesting); }

UiTestSupport::UiTestSupport(QObject* parent)
    : QObject(parent), utility_(new pqTestUtility(this)) {
  InitQtTestingResources();

  utility_->addEventObserver("xml", new AppQtTestingXMLEventObserver(utility_));
  utility_->addEventSource("xml", new AppQtTestingXMLEventSource(utility_));
}

void UiTestSupport::attachMenu(QMainWindow* window) {
  // 走 HostMenuBar 而不是 window->menuBar()：fluent
  // 路线的窗口壳会换掉菜单部件， menuBar() 从此返回空（见 tour_menu.h 的说明）
  QMenuBar* menu_bar = HostMenuBar(window);
  if (menu_bar == nullptr) {
    return;
  }
  QMenu* menu = menu_bar->addMenu(QStringLiteral("测试(&C)"));

  QAction* record_action = menu->addAction(QStringLiteral("录制事件(&R)..."));
  connect(record_action, &QAction::triggered, this, [this, window]() {
    // 文件名仍然得这里先给：pqRecordEventsDialog 是个控制面板（暂停/校验/加
    // 注释），自己不带文件选择。无参的 recordTests() 少了文件名会在
    // EventObservers.value(FileSuffix) 那里查不到、直接 return，连录制都不启动
    const QString file = QFileDialog::getSaveFileName(
        window, QStringLiteral("测试脚本另存为"), QString(),
        QStringLiteral("XML 文件 (*.xml)"));
    if (file.isEmpty()) {
      return;
    }

    // 录制靠全局事件过滤器捕获投递到窗口的事件，窗口必须处于活动状态
    QApplication::setActiveWindow(window);

    // 但**不能紧接着就开录**：库内部用 QApplication::activeWindow() 决定要不要
    // 弹那个控制面板，而刚刚关掉的文件对话框还没把激活交还回来，此刻读到的是空，
    // 面板就不出现（录制照常跑，只是没有面板）。延后一轮，让激活先落回去
    QTimer::singleShot(0, this,
                       [this, file]() { utility_->recordTests(file); });
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
