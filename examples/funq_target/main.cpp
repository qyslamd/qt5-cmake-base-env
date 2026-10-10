// funq 注入式 UI 测试的靶子程序。
//
// 与另外的示例的分工：tour_demo 演示引导系统，本示例只做一件事——
// 给 funq 提供一个「控件少而稳、路径可预测」的靶子。
//
// 为什么控件少而稳：funq 的控件路径优先取 objectName，取不到就退化成
// 「类名 + 同名兄弟序号」，而序号取决于创建顺序与布局重挂载，界面一动路径就
// 变（上一版当靶子的 gallery 就吃过这个亏，路径只能从运行中的进程里查）。
// 所以这里
// 每个被测控件都显式设 objectName，路径即 objectName，改界面不必重新探路径。
//
// 与 demo_app 的区别：demo_app 有登录框挡在前面、控件树随业务走；靶子无登录、
// 控件固定，测试用例可以只关心「注入 → 操作 → 断言」这条链路本身。

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "funq_target_window.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  FunqTargetWindow window;
  window.show();

  return app.exec();
}
