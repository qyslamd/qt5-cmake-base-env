#ifndef FILE_LIBUI_RADIAL_NAVIGATOR_H_
#define FILE_LIBUI_RADIAL_NAVIGATOR_H_

#include <functional>

#include <QList>
#include <QWidget>

class QParallelAnimationGroup;
class QVariantAnimation;

// 环形导航：若干圆形按钮均布于圆周（图标在上、文字在下，文字超宽自动
// 缩号），中央圆形开关展开/收起，带展开进度动画与逐项延迟。
// 按钮自带 onClicked 回调，点击直接执行动作（切换页面 / 打开对话框），
// 调用方无需维护索引映射。整体显示/隐藏由 setVisible 控制。
// 纯自绘，不依赖任何图片资源。
class RadialNavigator : public QWidget {
  Q_OBJECT
 public:
  explicit RadialNavigator(QWidget* parent = nullptr);
  ~RadialNavigator() override;

  struct Button {
    QPixmap icon;
    QString title;
    QString tooltip;
    QColor backgroundColor;
    std::function<void()> onClicked;
    bool hover = false;
    bool selected = false;
  };

  // 添加圆形按钮（onClicked 为点击动作）
  void addButton(const QPixmap& icon, const QString& title,
                 std::function<void()> onClicked,
                 const QString& tooltip = QString(),
                 const QColor& background = QColor());

  // 高亮指定按钮（页面对齐用）
  void setCurrentIndex(int index);

 protected:
  bool event(QEvent* event) override;
  bool eventFilter(QObject* obj, QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 private:
  void initAnimation();

  void toggleMenu();
  void updateHover(const QPoint& pos);

  // 几何
  QPointF buttonCenter(int index) const;
  qreal buttonRadius(int index) const;
  qreal buttonProgress(int index) const;  // 含逐项延迟的展开进度
  int buttonAt(const QPoint& pos) const;
  qreal baseRadius() const;
  qreal itemSize() const;

  // 绘制
  void drawGlow(QPainter* painter, const QPointF& center, qreal radius,
                const QColor& color);
  void drawBackdrop(QPainter* painter);
  void drawButton(QPainter* painter, int index);
  void drawSwitch(QPainter* painter);

  QList<Button> buttons_;
  bool menuOpen_ = false;
  int currentIndex_ = -1;
  bool switchHover_ = false;
  bool opening_ = false;  // 当前动画方向（true=展开）

  // 动画：展开进度 + 整体旋转角
  qreal progress_ = 0.0;
  qreal rotateDeg_ = 0.0;
  QParallelAnimationGroup* anim_group_ = nullptr;
  QVariantAnimation* anim_progress_ = nullptr;
  QVariantAnimation* anim_rotate_ = nullptr;

  static constexpr int kAnimationMs = 400;
  static constexpr qreal kStaggerMs = 20.0;  // 逐项延迟
};

#endif  // FILE_LIBUI_RADIAL_NAVIGATOR_H_
