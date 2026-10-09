#ifndef FILE_TOUR_HOMEPAGE_H_
#define FILE_TOUR_HOMEPAGE_H_

#include <QWidget>

class QCheckBox;
class QFrame;
class QHBoxLayout;
class QLabel;
class QMouseEvent;
class QPushButton;
class QShowEvent;
class QToolButton;

class TourController;
class TourTopic;

// 引导首页的主题卡片：图标 + 名称 + 描述。
//
// 外观**只有 QSS 一个出处**（`tour_default.qss` 里的 `#tourCard` 那几条）：
// 底色/边框/圆角/选中/占位都在那儿，这里只做 QSS 表达不了的按下-抬起配对
class TourCard : public QWidget {
  Q_OBJECT
 public:
  explicit TourCard(TourTopic* topic, QWidget* parent = nullptr);

  TourTopic* topic() const { return topic_; }
  bool isPlaceholder() const { return placeholder_; }
  void setSelected(bool selected);
  bool isSelected() const { return selected_; }

 signals:
  void clicked();

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 private:
  void initUi();

 private:
  QLabel* icon_label_ = nullptr;
  QLabel* title_label_ = nullptr;
  QLabel* desc_label_ = nullptr;
  TourTopic* topic_ = nullptr;
  bool placeholder_ = false;
  bool selected_ = false;
  bool pressed_ = false;
};

// 引导首页：盖在宿主窗口之上的覆盖层（黑半透明遮罩 + 居中卡片），靠 raise()
// 置顶，不用 QDialog 的事件循环。几何在 showEvent 里按父窗口算；
// exec() 只是阻塞式兼容接口
class TourHomePage : public QWidget {
  Q_OBJECT

 public:
  explicit TourHomePage(TourController* controller, QWidget* parent = nullptr);

  // 兼容 QDialog::exec() 用法：显示并阻塞到**页面自己关掉**（closeRequested），
  // 返回 0。等的不是"隐藏"——父窗口一藏、页面被 reparent、退出应用时子控件被
  // 隐藏都会走 hide 那条路，那时候用户根本没选
  int exec();

  // 首页标题与副标题。默认取宿主的 applicationName()——引导是通用库，不该写死
  // 产品名；产品名与程序名不一致时用这个改
  void setHeading(const QString& title, const QString& subtitle);

 signals:
  // 页面要关了：**只有 hideHomePage() 发**（点卡片 / 全部 / 关闭都经过它）。
  // exec() 的局部事件循环等的是它
  void closeRequested();

 protected:
  void showEvent(QShowEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  void initUi();
  void initStyleSheet();
  void initSignals();
  void rebuildCards();
  void hideHomePage();
  void startTopic(TourTopic* topic);

 private:
  QFrame* panel_ = nullptr;
  QLabel* title_label_ = nullptr;
  QLabel* sub_label_ = nullptr;
  QToolButton* close_btn_ = nullptr;
  QPushButton* all_btn_ = nullptr;
  QCheckBox* auto_check_ = nullptr;
  QHBoxLayout* cards_layout_ = nullptr;
  TourController* controller_ = nullptr;
};
#endif  // FILE_TOUR_HOMEPAGE_H_
