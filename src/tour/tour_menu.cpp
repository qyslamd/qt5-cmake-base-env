#include "tour_menu.h"

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QString>

#include <tour_controller.h>
#include <tour_homepage.h>

TourMenu::TourMenu(TourController* controller, QObject* parent)
    : QObject(parent), controller_(controller) {}

void TourMenu::attachMenu(QMainWindow* window, GuideTopic* tour_topic) {
  if (window == nullptr) {
    return;
  }

  QMenu* menu = window->menuBar()->addMenu(QStringLiteral("功能引导"));
  menu->setObjectName(QStringLiteral("menuTour"));

  QAction* home_action = menu->addAction(QStringLiteral("打开引导主页"));
  home_action->setObjectName(QStringLiteral("actionTourHome"));
  connect(home_action, &QAction::triggered, this, [this, window]() {
    if (controller_ == nullptr) {
      return;
    }
    // 懒建一次后复用。父级给主窗口，随窗口销毁
    if (home_page_ == nullptr) {
      home_page_ = new TourHomePage(controller_, window);
    }
    // exec() 是"显示 + 阻塞到关闭"：主页里点卡片会先隐藏自己（hideEvent 发
    // closed），局部事件循环随之退出，控制器才开播——所以这里阻塞不会挡住引导
    home_page_->exec();
  });

  // 直接播指定主题的快捷入口，省掉主页那一步
  QAction* tour_action = menu->addAction(QStringLiteral("模板导览"));
  tour_action->setObjectName(QStringLiteral("actionTemplateTour"));
  tour_action->setEnabled(tour_topic != nullptr);
  connect(tour_action, &QAction::triggered, this, [this, tour_topic]() {
    if (controller_ == nullptr || tour_topic == nullptr) {
      return;
    }
    controller_->startOne(tour_topic, /*autoMode=*/true);
  });
}
