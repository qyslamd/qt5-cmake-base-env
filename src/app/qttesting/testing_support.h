#ifndef FILE_APP_QTTESTING_TESTING_SUPPORT_H_
#define FILE_APP_QTTESTING_TESTING_SUPPORT_H_

#include <QObject>
#include <QString>

class QMainWindow;
class pqTestUtility;

// UI 测试支撑：把 QtTesting 的录制/回放接到本应用上。
//
// 只有 BUILD_QT_TESTING=ON 的构建才编译本文件（见 src/app/CMakeLists.txt）。
// QtTesting 是静态库，开了就整套链进 exe，不需要时用该开关关掉。
//
// 两个入口：
//   录制：主窗口菜单「测试 → 录制事件」，产出 XML 事件脚本
//   回放：命令行 app --play <file>，跳过登录、进主窗口后回放，按结果设退出码
//
// 回放为何跳过登录：录制入口在主窗口菜单里，也就是说录制总是从「已登录」的
// 状态开始。回放只有从同一状态起步，脚本里的控件才有对应的对象存在。
class UiTestSupport : public QObject {
  Q_OBJECT

 public:
  explicit UiTestSupport(QObject* parent = nullptr);

  // 在主窗口菜单栏挂「测试」菜单（录制与回放两个入口）
  void attachMenu(QMainWindow* window);

  // 回放事件脚本。阻塞直到播完，返回是否全部成功
  bool playFile(const QString& file);

 private:
  pqTestUtility* utility_ = nullptr;
};

#endif  // FILE_APP_QTTESTING_TESTING_SUPPORT_H_
