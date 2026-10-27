#ifndef STYLE_SHEET_DEMO_PROGRESS_PAGE_H_
#define STYLE_SHEET_DEMO_PROGRESS_PAGE_H_

#include <QWidget>

// 进度与滑块演示：QProgressBar 的 0% / 50% / 100% / disabled，
// QSlider 的水平与垂直两种方向。
class ProgressPage : public QWidget {
 public:
  explicit ProgressPage(QWidget* parent = nullptr);

 private:
  void initUi();
};

#endif  // STYLE_SHEET_DEMO_PROGRESS_PAGE_H_