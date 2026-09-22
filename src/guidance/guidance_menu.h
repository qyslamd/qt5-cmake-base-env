#ifndef FILE_GUIDANCE_MENU_H_
#define FILE_GUIDANCE_MENU_H_

#include <QObject>
#include <QPointer>

class QMainWindow;
class GuidanceController;
class GuidanceHomePage;

// 给 QMainWindow 宿主用的便利层：在菜单栏挂一个「功能引导」入口
// （打开主题主页 / 直接播第一条主题）。
//
// 引擎本身不知道菜单这回事——不想用菜单的宿主直接用 GuidanceController 与
// GuidanceHomePage 即可（例如宿主窗口不是 QMainWindow、没有菜单栏时）
class GuidanceMenu : public QObject {
  Q_OBJECT

 public:
  explicit GuidanceMenu(GuidanceController* controller,
                        QObject* parent = nullptr);

  void attachMenu(QMainWindow* window);

 private:
  GuidanceController* controller_ = nullptr;
  // 主题主页：懒建一次后复用，父级是主窗口，随窗口销毁
  QPointer<GuidanceHomePage> home_page_;
};

#endif  // FILE_GUIDANCE_MENU_H_
