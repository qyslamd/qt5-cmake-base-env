#ifndef FILE_APP_SPLASH_SCREEN_H_
#define FILE_APP_SPLASH_SCREEN_H_

#include <QWidget>

class QLabel;
class QProgressBar;

// 启动画面：登录通过后覆盖主界面创建阶段，提供 logo + 状态文本 + 进度条。
// 刻意不引用 MainWindow，也不要求主窗口回报进度——模板不侵入用户的主窗口
// 设计，进度由调用方（main）在各自的初始化步骤处显式驱动。
class SplashScreen : public QWidget {
  Q_OBJECT

 public:
  explicit SplashScreen(QWidget* parent = nullptr);

  // 更新当前步骤的说明文本
  void setStatusText(const QString& text);
  // 更新进度条，percent 取值 0-100
  void setProgress(int percent);
  // 结束启动阶段：隐藏自身。之后主界面如何显示由调用方决定
  void finish();

 protected:
  void showEvent(QShowEvent* event) override;
  // 吞掉鼠标与键盘事件，避免启动期间的操作落到尚未就绪的主界面上
  void mousePressEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void initUi();

  QLabel* logo_label_ = nullptr;
  QLabel* title_label_ = nullptr;
  QLabel* status_label_ = nullptr;
  QProgressBar* progress_bar_ = nullptr;
};

#endif  // FILE_APP_SPLASH_SCREEN_H_
