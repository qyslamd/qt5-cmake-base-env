#ifndef EXAMPLES_LIBUI_GALLERY_GALLERY_WINDOW_H_
#define EXAMPLES_LIBUI_GALLERY_GALLERY_WINDOW_H_

#include <QWidget>

class QComboBox;
class QPushButton;
class QTabWidget;
class QTimer;
class LogOutputPanel;
class SplashScreen;
class SwitchButton;
class TabBarStyle;

// libui 部件演示：上半部每个部件占一个标签页，下半部是日志面板。
// 部件的操作结果走 LOG_INFO 输出到日志，既能看到交互，也能顺带了解
// 日志系统的用法。新增部件时加一个 createXxxPage() 并注册成标签页即可。
//
// 标签栏外观不单开一页：主标签栏本身挂着 TabBarStyle，顶部的下拉框
// 切换外观即完成演示，切换后所有标签页同步换形
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

  // 切换主标签栏外观：换掉 TabBarStyle 持有的 TabAppearance，
  // 并套用该外观自己的主题微调
  void applyAppearance(int index);

  QPushButton* splash_button_ = nullptr;
  SwitchButton* switch_button_ = nullptr;
  LogOutputPanel* log_panel_ = nullptr;
  QComboBox* appearance_combo_ = nullptr;

  // 主标签栏与挂在它上面的外观样式
  QTabWidget* tabs_ = nullptr;
  TabBarStyle* tab_style_ = nullptr;

  SplashScreen* splash_ = nullptr;
  QTimer* splash_timer_ = nullptr;
  int splash_step_ = 0;
};

#endif  // EXAMPLES_LIBUI_GALLERY_GALLERY_WINDOW_H_
