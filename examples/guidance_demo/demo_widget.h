#ifndef FILE_EXAMPLES_GUIDANCE_DEMO_DEMO_WIDGET_H_
#define FILE_EXAMPLES_GUIDANCE_DEMO_DEMO_WIDGET_H_

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

class GuidanceController;
class GuidanceHomePage;

// 引导库的演示窗口。刻意用一个普通 QWidget——没有 QMainWindow、没有菜单栏：
// 库的核心 API（Controller + HomePage）对宿主是什么窗口没有要求，菜单那套
// （GuidanceSupport::attachMenu）只是给主窗口宿主准备的便利层。
//
// 窗口里放几个真实控件，作为引导主题的高亮目标
class DemoWidget : public QWidget {
  Q_OBJECT

 public:
  explicit DemoWidget(QWidget* parent = nullptr);

 private slots:
  void onOpenHomePage();

 private:
  void initUi();
  void initSignals();

  QLineEdit* name_edit_ = nullptr;
  QComboBox* mode_combo_ = nullptr;
  QCheckBox* verbose_check_ = nullptr;
  QPushButton* apply_btn_ = nullptr;
  QPushButton* guide_btn_ = nullptr;
  QLabel* status_label_ = nullptr;

  // 引导引擎入口。它自己持有演示层，窗口只负责给 viewport 与主题
  GuidanceController* controller_ = nullptr;
  // 主题主页：懒建一次后复用，父级是本窗口，随窗口销毁
  GuidanceHomePage* home_page_ = nullptr;
};

#endif  // FILE_EXAMPLES_GUIDANCE_DEMO_DEMO_WIDGET_H_
