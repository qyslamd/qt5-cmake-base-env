#ifndef EXAMPLES_LIBUI_GALLERY_GALLERY_WINDOW_H_
#define EXAMPLES_LIBUI_GALLERY_GALLERY_WINDOW_H_

#include <QWidget>

class QPushButton;
class QTimer;
class QWidget;
class LogOutputPanel;
class SplashScreen;
class SwitchButton;

// libui 部件演示：上半部每个部件占一个标签页，下半部是日志面板。
// 部件的操作结果走 LOG_INFO 输出到日志，既能看到交互，也能顺带了解
// 日志系统的用法。新增部件时加一个 createXxxPage() 并注册成标签页即可。
class GalleryWindow : public QWidget {
  Q_OBJECT

 public:
  explicit GalleryWindow(QWidget* parent = nullptr);

 private slots:
  // 演示 SplashScreen：它是独立窗口而非可嵌入控件，故由按钮触发
  void onShowSplash();
  void onSplashStep();

 private:
  void initUi();
  void initSignals();

  // 各部件一页：页内是用法说明加上部件本身
  QWidget* createSwitchButtonPage();
  QWidget* createSectorMenuPage();
  QWidget* createRadialNavigatorPage();

  QPushButton* splash_button_ = nullptr;
  SwitchButton* switch_button_ = nullptr;
  LogOutputPanel* log_panel_ = nullptr;

  SplashScreen* splash_ = nullptr;
  QTimer* splash_timer_ = nullptr;
  int splash_step_ = 0;
};

#endif  // EXAMPLES_LIBUI_GALLERY_GALLERY_WINDOW_H_
