#ifndef FILE_EXAMPLES_FUNQ_TARGET_FUNQ_TARGET_WINDOW_H_
#define FILE_EXAMPLES_FUNQ_TARGET_FUNQ_TARGET_WINDOW_H_

#include <QString>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTabWidget;

// funq 测试靶子的主窗口：三个页签，页面里的控件都设了 objectName。
//
// 联动关系是刻意留的：name_edit 的文本变化会写进 status_label，这样
// 「写一个属性 → 观察另一处的界面变化」这条用例有真实业务含义，而不是
// 把属性原样读回来（gallery 的用例只能做后者）。
//
// submit_count_ 用 Q_PROPERTY 暴露：点击用例要断言的是「事件真的送到了
// 控件上」，一个自增计数比读取按钮自身的 checked 更能说明这件事。
class FunqTargetWindow : public QWidget {
  Q_OBJECT
  Q_PROPERTY(int submit_count READ submitCount)

 public:
  explicit FunqTargetWindow(QWidget* parent = nullptr);

  int submitCount() const { return submit_count_; }

 private slots:
  void onNameChanged(const QString& text);
  void onSubmitClicked();
  void onResetClicked();

 private:
  void initUi();
  void initSignals();

  QTabWidget* tabs_ = nullptr;
  QLineEdit* name_edit_ = nullptr;
  QComboBox* level_combo_ = nullptr;
  QCheckBox* agree_check_ = nullptr;
  QLabel* status_label_ = nullptr;
  QProgressBar* progress_bar_ = nullptr;
  QLabel* click_label_ = nullptr;
  QPushButton* submit_button_ = nullptr;
  QPushButton* reset_button_ = nullptr;
  int submit_count_ = 0;
};

#endif  // FILE_EXAMPLES_FUNQ_TARGET_FUNQ_TARGET_WINDOW_H_
