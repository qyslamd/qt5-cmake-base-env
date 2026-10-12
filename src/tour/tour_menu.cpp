#include "tour_menu.h"

#include <tour_controller.h>
#include <tour_homepage.h>

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QString>

QMenuBar* HostMenuBar(QMainWindow* window) {
  if (window == nullptr) {
    return nullptr;
  }
  // 菜单栏本身就是菜单部件：QSS / SARibbon 路线走这里（menuWidget 是纯 getter，
  // 不会新建任何东西）
  if (QMenuBar* bar = qobject_cast<QMenuBar*>(window->menuWidget())) {
    return bar;
  }
  // 菜单部件为空 = 菜单栏还没建。这时 menuBar() 是安全的（它会建一个装上）
  if (window->menuWidget() == nullptr) {
    return window->menuBar();
  }
  // 剩下这种就是 fluent 路线：菜单部件是窗口壳的 chrome header（普通
  // QWidget）， 菜单栏成了它的子控件。
  // **此处绝不能调 window->menuBar()**：Qt 5.15 的实现是「菜单部件不是
  // QMenuBar 就新建一个 QMenuBar 装上去」，实测这条路径直接崩（转储里
  // QMenuBar::cornerWidget 以 this=0 被调用、故障地址 0x4），而且就算不崩也会
  // 把 Fluent 的 chrome header 顶掉
  return window->findChild<QMenuBar*>();
}

TourMenu::TourMenu(TourController* controller, QObject* parent)
    : QObject(parent), controller_(controller) {}

void TourMenu::attachMenu(QMainWindow* window, TourTopic* tour_topic) {
  if (window == nullptr) {
    return;
  }

  QMenuBar* menu_bar = HostMenuBar(window);
  if (menu_bar == nullptr) {
    return;  // 没有菜单栏可挂（宿主自行改了窗口结构），静默跳过
  }
  QMenu* menu = menu_bar->addMenu(QStringLiteral("功能引导"));
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
    controller_->startOne(tour_topic, /*auto_mode=*/true);
  });
}
