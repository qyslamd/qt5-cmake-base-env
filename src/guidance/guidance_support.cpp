#include "guidance_support.h"

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>

#include "guidance_controller.h"
#include "guidance_homepage.h"
#include "guidance_presentation.h"

GuidanceSupport::GuidanceSupport(GuidanceController* controller,
                                 QObject* parent)
    : QObject(parent),
      controller_(controller),
      presentation_(controller != nullptr ? controller->presentation()
                                          : nullptr) {}

void GuidanceSupport::attachMenu(QMainWindow* window) {
  if (window == nullptr) {
    return;
  }

  QMenu* menu = window->menuBar()->addMenu(QStringLiteral("功能引导"));
  menu->setObjectName(QStringLiteral("menuGuidance"));

  QAction* home_action = menu->addAction(QStringLiteral("打开引导主页"));
  home_action->setObjectName(QStringLiteral("actionGuidanceHome"));
  connect(home_action, &QAction::triggered, this, [this, window]() {
    if (controller_ == nullptr) {
      return;
    }
    // 懒建一次后复用。父级给主窗口，随窗口销毁
    if (home_page_ == nullptr) {
      home_page_ = new GuidanceHomePage(controller_, window);
    }
    // exec() 是"显示 + 阻塞到关闭"：主页里点卡片会先隐藏自己（hideEvent 发
    // closed），局部事件循环随之退出，控制器才开播——所以这里阻塞不会挡住引导
    home_page_->exec();
  });

  // 直接播第一条主题的快捷入口，省掉主页那一步
  QAction* tour_action = menu->addAction(QStringLiteral("模板导览"));
  tour_action->setObjectName(QStringLiteral("actionTemplateTour"));
  connect(tour_action, &QAction::triggered, this, [this]() {
    if (controller_ == nullptr || controller_->flows().isEmpty()) {
      return;
    }
    controller_->startOne(controller_->flows().first(), /*autoMode=*/true);
  });
}

void GuidanceSupport::showHighlight(const QString& path, const QString& title,
                                    const QString& text) {
  // 桥接层尚未实现（设计见下方注释）。形参先显式吞掉：声明里有它们，/W4 下
  // 不吞会报 C4100
  Q_UNUSED(path);
  Q_UNUSED(title);
  Q_UNUSED(text);

  // TODO:
  /*
if (!presentation_)
return;

// 切到主线程（funq 的 onMessageReceived 本来就在主线程，
// 但防御性转一下，万一将来 socket 线程变了）
QMetaObject::invokeMethod(
presentation_,
[=]() {
QObject* obj = ObjectPath::findObject(path);
QWidget* target = qobject_cast<QWidget*>(obj);
if (!target) {
qWarning() << "GuidanceBridge: path not found:" << path;
return;
}
presentation_->showHighlight(target, title, text);

// 模态对话框打开时，主窗口被挡，高亮看不见。showHighlight 里判断目标的归属窗口
QWidget* target_window = target->window();
QWidget* current_parent = qobject_cast<QWidget*>(presentation_->parent());

if (target_window && target_window != current_parent) {
    presentation_->setParent(target_window);
    presentation_->setGeometry(target_window->rect());
}

// 坑：模态对话框关闭时，presentation 的父级被销毁，presentation 跟着销毁。你的
// presentation 由 controller 持有、setParent 之后会被 Qt
// 的父子机制接管析构——这是个陷阱。
// presentation 的父级始终设为 nullptr，用 QPointer 追踪当前 host，host 销毁时把
// presentation 重新挂回主窗口。或者更简单——presentation 不做 Qt 父子，用
// raise() +
// setGeometry 手动管理层级。

},
Qt::QueuedConnection);
*/
}

void GuidanceSupport::hideHighlight() {
  // TODO: 与 showHighlight 同批实现（funq -> 演示层 的清理侧）
}