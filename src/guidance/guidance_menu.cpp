#include "guidance_menu.h"

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QString>

#include <guidance_controller.h>
#include <guidance_homepage.h>

GuidanceMenu::GuidanceMenu(GuidanceController* controller, QObject* parent)
    : QObject(parent), controller_(controller) {}

void GuidanceMenu::attachMenu(QMainWindow* window) {
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
