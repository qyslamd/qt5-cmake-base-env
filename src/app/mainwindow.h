#ifndef FILE_APP_MAIN_WINDOW_H_
#define FILE_APP_MAIN_WINDOW_H_

#include <QMainWindow>
#include <QVector>

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 private:
  void initUi();
  void initSignals();
};

#endif  // FILE_APP_MAIN_WINDOW_H_
