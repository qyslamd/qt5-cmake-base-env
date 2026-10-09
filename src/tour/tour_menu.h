#ifndef FILE_TOUR_MENU_H_
#define FILE_TOUR_MENU_H_

#include <QObject>
#include <QPointer>

class QMainWindow;
class TourController;
class TourTopic;
class TourHomePage;

// 给 QMainWindow 宿主用的便利层：在菜单栏挂一个「功能引导」入口。
// 引擎本身不知道菜单这回事——不想用菜单的宿主直接用 TourController 与
// TourHomePage 即可
class TourMenu : public QObject {
  Q_OBJECT

 public:
  explicit TourMenu(TourController* controller, QObject* parent = nullptr);

  // tour_topic：那条"直接播"的主题，**由宿主显式传入**（换成取
  // topics().first() 就成了"谁先注册谁是"，与菜单文字对不上）。
  // 传 nullptr 则该菜单项置灰
  void attachMenu(QMainWindow* window, TourTopic* tour_topic);

 private:
  TourController* controller_ = nullptr;
  // 主题主页：懒建一次后复用，父级是主窗口，随窗口销毁
  QPointer<TourHomePage> home_page_;
};

#endif  // FILE_TOUR_MENU_H_
