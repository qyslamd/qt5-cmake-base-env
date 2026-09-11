#ifndef FILE_APP_MAIN_WINDOW_H_
#define FILE_APP_MAIN_WINDOW_H_

#include <QMainWindow>

class LogWindow;

// 主窗口骨架：仅提供菜单栏入口与占位中央控件，不承载具体业务。
// 模板使用者可自由替换中央控件与菜单结构，日志窗口、设置、关于均为
// 独立类，删除入口即可去掉对应功能。
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 private slots:
  void onShowLogWindow();
  void onShowSettings();
  void onShowAbout();

 private:
  void initUi();
  void initSignals();

  LogWindow* log_window_ = nullptr;
};

#endif  // FILE_APP_MAIN_WINDOW_H_
