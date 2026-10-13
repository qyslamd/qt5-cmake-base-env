#ifndef FILE_APP_LOG_WINDOW_H_
#define FILE_APP_LOG_WINDOW_H_

#include <QDialog>

class QPushButton;

class LogOutputPanel;

// 日志窗口：把 LogOutputPanel 装进独立窗口，供菜单随时唤出。
// 独立成类而非塞进主窗口，模板使用者可整类删除而不影响主界面结构。
class LogWindow : public QDialog {
  Q_OBJECT

 public:
  explicit LogWindow(QWidget* parent = nullptr);

 private:
  void initUi();
  void initSignals();

  LogOutputPanel* panel_ = nullptr;
  QPushButton* close_btn_ = nullptr;
};

#endif  // FILE_APP_LOG_WINDOW_H_
