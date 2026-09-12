#ifndef EXAMPLES_LIBUI_GALLERY_GALLERY_WINDOW_H_
#define EXAMPLES_LIBUI_GALLERY_GALLERY_WINDOW_H_

#include <QWidget>

class QPlainTextEdit;
class QPushButton;
class QTimer;
class RadialNavigator;
class SectorMenu;
class SplashScreen;
class SwitchButton;

// libui 部件集中演示：把各部件摆在一处，操作结果输出到右侧事件区，
// 便于对照各部件头文件中的用法说明理解其接口。
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

  void appendEvent(const QString& text);

  QPlainTextEdit* event_view_ = nullptr;
  QPushButton* splash_button_ = nullptr;
  SwitchButton* switch_button_ = nullptr;
  SectorMenu* sector_menu_ = nullptr;
  RadialNavigator* navigator_ = nullptr;

  SplashScreen* splash_ = nullptr;
  QTimer* splash_timer_ = nullptr;
  int splash_step_ = 0;
};

#endif  // EXAMPLES_LIBUI_GALLERY_GALLERY_WINDOW_H_
