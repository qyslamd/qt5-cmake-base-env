#ifndef FILE_TOUR_MENU_H_
#define FILE_TOUR_MENU_H_

#include <QObject>
#include <QPointer>

class QMainWindow;
class TourController;
class GuideTopic;
class TourHomePage;

// 给 QMainWindow 宿主用的便利层：在菜单栏挂一个「功能引导」入口
// （打开主题主页 / 直接播指定的一条主题）。
//
// 引擎本身不知道菜单这回事——不想用菜单的宿主直接用 TourController 与
// TourHomePage 即可（例如宿主窗口不是 QMainWindow、没有菜单栏时）
class TourMenu : public QObject {
  Q_OBJECT

 public:
  explicit TourMenu(TourController* controller,
                        QObject* parent = nullptr);

  // tour_topic：「模板导览」那一条直接播的主题，**由宿主显式传入**。
  // 原先这里取 topics().first()——菜单文字与"第几条"是两回事，谁先注册谁就
  // "是"模板导览，纯靠注册顺序维系；多注册一条主题就会播错。传 nullptr
  // 则该菜单项置灰
  void attachMenu(QMainWindow* window, GuideTopic* tour_topic);

 private:
  TourController* controller_ = nullptr;
  // 主题主页：懒建一次后复用，父级是主窗口，随窗口销毁
  QPointer<TourHomePage> home_page_;
};

#endif  // FILE_TOUR_MENU_H_
