#ifndef FILE_LIBUI_SECTOR_MENU_H_
#define FILE_LIBUI_SECTOR_MENU_H_

#include <functional>

#include <QColor>
#include <QList>
#include <QPainterPath>
#include <QString>
#include <QWidget>

class QPainter;

// 环形扇区菜单：若干扇区沿圆周均布，每个扇区对应一个动作。
//
// 扇区由外弧、内弧与两条径向边构成，用 QPainterPath::arcTo 拼出；标题沿
// 扇区中弧排布，做法是把坐标系旋转到扇区中心后按正立横排绘制，避免逐字
// 旋转带来的阅读顺序问题，文字过宽时自动缩小字号。
//
// 悬停高亮、点击触发对应动作、鼠标离开清除高亮。纯自绘，不依赖图片资源。
class SectorMenu : public QWidget {
  Q_OBJECT

 public:
  explicit SectorMenu(QWidget* parent = nullptr);

  struct Item {
    QString title;
    QColor background;  // 无效时用默认半透明灰
    QColor text_color;  // 无效时用白色
    std::function<void()> on_clicked;
    bool hover = false;
  };

  // 追加一个扇区。on_clicked 可为空（只作显示）
  void addItem(const QString& title,
               std::function<void()> on_clicked = nullptr,
               const QColor& background = QColor(),
               const QColor& text_color = QColor());

  // 内外半径占控件半径的比例，决定环的宽度
  void setRadiusRatio(qreal outer_ratio, qreal inner_ratio);

  // 起始角度，单位为度；默认 180 即自左侧开始顺时针排布
  void setStartAngle(qreal degrees);

  // 当前悬停的扇区索引，未悬停为 -1
  int hoverIndex() const { return hover_index_; }

  int count() const { return items_.size(); }

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  void initUi();

  void drawItems(QPainter* painter);
  void drawLabel(QPainter* painter, const Item& item, int index);
  // 第 index 个扇区的路径，半径由当前控件尺寸与半径比推出
  QPainterPath sectorPath(int index) const;
  // 坐标落在哪个扇区；未命中返回 -1
  int itemAt(const QPoint& pos) const;

  QList<Item> items_;
  qreal outer_ratio_ = 0.95;
  qreal inner_ratio_ = 0.55;
  qreal start_angle_ = 180.0;
  int hover_index_ = -1;
};

#endif  // FILE_LIBUI_SECTOR_MENU_H_
