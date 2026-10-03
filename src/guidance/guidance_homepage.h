#ifndef FILE_GUIDANCE_HOMEPAGE_H_
#define FILE_GUIDANCE_HOMEPAGE_H_

#include <QWidget>

class QCheckBox;
class QFrame;
class QHBoxLayout;
class QLabel;
class QMouseEvent;
class QPushButton;
class QShowEvent;
class QToolButton;

class GuidanceController;
class GuidanceFlow;

// 引导首页主题卡片：图标 + 名称 + 描述。
//
// **卡片的样式只有 QSS 一个出处**（`guidance_default.qss` 里的
// `#guidanceCard` 那几条）：底色/边框/圆角是规则本体，hover 走 `:hover`
// 伪状态，选中与占位走动态属性 `selected` / `placeholder`。
//
// 这里曾经同时自绘一套底色与边框，注释还写着"QSS 覆盖、无 QSS 时自绘兜底"
// ——不成立：自绘是无条件的，且画在 QSS 之后，实际是它盖住 QSS。改 QSS 的人
// 会发现没反应。现在只保留 QSS 表达不了的东西：点击的按下/抬起配对判定
class GuidanceCard : public QWidget {
  Q_OBJECT
 public:
  explicit GuidanceCard(GuidanceFlow* flow, QWidget* parent = nullptr);

  GuidanceFlow* flow() const { return flow_; }
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
  GuidanceFlow* flow_ = nullptr;
  bool placeholder_ = false;
  bool selected_ = false;
  bool pressed_ = false;
};

// 引导首页：覆盖在宿主窗口之上的子控件覆盖层，
// 黑半透明遮罩 + 居中卡片。通过 raise() 置顶，不用 QDialog 的事件循环。
// - 显示/隐藏：show() / hide()（exec() 保留阻塞式兼容接口）
// - 父窗口尺寸变化：showEvent 中按 parentWidget()->rect() 同步几何
// - 遮罩颜色自绘
class GuidanceHomePage : public QWidget {
  Q_OBJECT

 public:
  explicit GuidanceHomePage(GuidanceController* controller,
                            QWidget* parent = nullptr);

  // 兼容旧 QDialog::exec() 用法：显示并阻塞直到隐藏，返回 0。
  int exec();

  // 首页标题与副标题。**默认取宿主的 applicationName()**——引导是通用库，
  // 不该写死某个产品的名字（原先写死了某个老 Demo 的叫法，本仓里对不上任何
  // 东西）。宿主的产品名与程序名不一致时，用这个改
  void setHeading(const QString& title, const QString& subtitle);

 signals:
  void closed();

 protected:
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  void initUi();
  void initStyleSheet();
  void initSignals();
  void rebuildCards();
  void hideHomePage();
  void startFlow(GuidanceFlow* flow);

 private:
  QFrame* panel_ = nullptr;
  QLabel* title_label_ = nullptr;
  QLabel* sub_label_ = nullptr;
  QToolButton* close_btn_ = nullptr;
  QPushButton* all_btn_ = nullptr;
  QCheckBox* auto_check_ = nullptr;
  QHBoxLayout* cards_layout_ = nullptr;
  GuidanceController* controller_ = nullptr;
};
#endif  // FILE_GUIDANCE_HOMEPAGE_H_
