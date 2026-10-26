#ifndef FILE_APP_MAIN_WINDOW_H_
#define FILE_APP_MAIN_WINDOW_H_

// clang-format off
// 基类三选一（由界面路线与 Ribbon 开关决定，见 src/app/CMakeLists.txt）：
//   fluent —— QMainWindow + Fluent 标题栏（窗口壳挂在 initFluentFrame 里）
//   其余   —— 开了 SARibbon 用 SARibbonMainWindow（它把菜单栏那一行换成
//             功能区），否则退回 QMainWindow + 普通菜单栏
// 三种构建下 MainWindow 的用法完全一致，差异全收在 mainwindow.cpp 的
// initRibbon / initMenuBar / initFluentFrame 里
#if defined(APP_UI_FLUENT)
#include <QMainWindow>
using MainWindowBase = QMainWindow;
#elif defined(APP_ENABLE_RIBBON)
#include <SARibbonMainWindow.h>
using MainWindowBase = SARibbonMainWindow;
#else
#include <QMainWindow>
using MainWindowBase = QMainWindow;
#endif
// clang-format on

class QAction;
class LogWindow;
class FluentWindowFrame;

// 主窗口骨架：入口（功能区或菜单栏）+ 占位中央控件，不承载具体业务。
// 模板使用者可自由替换中央控件与入口结构，日志窗口、设置、关于均为独立类，
// 删除入口即可去掉对应功能。
class MainWindow : public MainWindowBase {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 private slots:
  void onShowLogWindow();
  void onShowSettings();
  void onShowAbout();
  void onMakeCrash();

 private:
  // 三步走：建动作（两种形态共用）→ 挂到功能区或菜单栏 → 铺中央控件
  void initUi();
  void initSignals();
  void createActions();
#if defined(APP_UI_FLUENT)
  // fluent 路线复用普通菜单栏（放进 Fluent 标题栏下方那一块），再加窗口壳
  void initMenuBar();
  void initFluentFrame();
#elif defined(APP_ENABLE_RIBBON)
  void initRibbon();
#else
  void initMenuBar();
#endif
  void initCentralWidget();

  // 动作声明成成员而不是就地挂到菜单上：功能区与菜单栏共用同一批动作，
  // 分开建、分开挂，谁先谁后都说得清
  QAction* quit_action_ = nullptr;
  QAction* log_action_ = nullptr;
  QAction* make_crash_action_ = nullptr;
  QAction* settings_action_ = nullptr;
  QAction* about_action_ = nullptr;

  LogWindow* log_window_ = nullptr;
#ifdef APP_UI_FLUENT
  // 窗口壳（QWK 无边框 + Fluent 标题栏）。宿主窗口是它的 QObject 父级
  FluentWindowFrame* fluent_frame_ = nullptr;
#endif
};

#endif  // FILE_APP_MAIN_WINDOW_H_
