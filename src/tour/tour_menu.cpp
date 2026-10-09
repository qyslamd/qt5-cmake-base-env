#include "tour_menu.h"

#include <tour_controller.h>
#include <tour_homepage.h>

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QString>

TourMenu::TourMenu(TourController* controller, QObject* parent)
    : QObject(parent), controller_(controller) {}

void TourMenu::attachMenu(QMainWindow* window, TourTopic* tour_topic) {
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
    if (home_page_ == nullptr) {
      home_page_ = new TourHomePage(controller_, window);
    }
    // exec() 阻塞到主页自己关掉（hideHomePage 发 closeRequested），而点卡片
    // 是"先关页、再开播"，所以这里不会挡住引导
    home_page_->exec();
  });

  // 直接播指定主题的快捷入口，省掉主页那一步。文案取**主题自己的名字**——
  // 写死一个名字，宿主换了主题之后菜单就与内容对不上了
  QAction* tour_action = menu->addAction(
      tour_topic != nullptr ? tour_topic->name() : QStringLiteral("导览"));
  tour_action->setObjectName(QStringLiteral("actionTour"));
  tour_action->setEnabled(tour_topic != nullptr);
  connect(tour_action, &QAction::triggered, this, [this, tour_topic]() {
    if (controller_ == nullptr || tour_topic == nullptr) {
      return;
    }
    controller_->startOne(tour_topic, /*autoMode=*/true);
  });
}
