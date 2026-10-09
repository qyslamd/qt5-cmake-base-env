#ifndef FILE_TOUR_OVERLAY_MASK_H_
#define FILE_TOUR_OVERLAY_MASK_H_

#include <QList>
#include <QObject>
#include <QPixmap>
#include <QRect>

class QPainter;
class QVariantAnimation;

// 遮罩：半透明底 + 挖出来的洞 + 洞沿的虚线框 + 图示步那张图。
//
// "高亮在哪儿"只有这一类持有：Highlight 是**要去的地方**，shown_anchor_ 是
// **此刻显示中的框**（动画的输出）。画什么、命不命中都在这里收口
class TourOverlayMask : public QObject {
  Q_OBJECT

 public:
  explicit TourOverlayMask(QObject* parent = nullptr);

  // 换高亮。animated=true 从"此刻显示中的框"挪过去；给了 extras 的一步不走
  // 动画（设计 D2：并集难看），直接对齐
  void setHighlight(const QRect& anchor, const QList<QRect>& extras,
                    bool animated);
  // 图示步那张图（贴在锚点那块上）。紧随其后总是 setHighlight
  void setPixmap(const QPixmap& pixmap);
  void clear();

  // 本步的**靶**：气泡摆位与箭头都用它。它不是"此刻显示中的框"——那是
  // shown_anchor_，动画跑着时两者不同，只给绘制与命中用
  QRect anchor() const { return highlight_.anchor; }
  // 一共框了几处（含锚点）；空锚点（纯讲解步）不算
  int count() const { return highlight_.count(); }

  // 点在靶上？**逐个矩形判，不合并并集**（设计 D3）：多靶时两块之间那块空白
  // 也算"点在目标上"的话，遮罩就永远关不掉了（点空白 = 退出）
  bool hitTarget(const QPoint& pos) const;

  // 画整块遮罩。viewport 是遮罩自己的矩形——本类不持有控件
  void paint(QPainter* painter, const QRect& viewport) const;

 signals:
  // 该重画了（换高亮、清空、动画的每一帧）
  void repaintRequested();

 private:
  // 当前这一步要高亮的框：锚点 + 其余要一起框住的靶，**一处真相**
  struct Highlight {
    // 锚点：气泡与三角箭头挂在它旁边。可以为空（空目标步是纯讲解）
    QRect anchor;
    // 其余要一起框住的靶（空矩形不进来）
    QList<QRect> extras;

    int count() const { return (anchor.isNull() ? 0 : 1) + extras.size(); }
  };

  // 命中与绘制共用的矩形表：**显示中的**锚点 + 额外靶
  QList<QRect> TargetRects() const;

  // 洞比靶往外扩这么多（框线也画在这一圈上）、框线宽度、动画时长
  static constexpr int kMargin = 10;
  static constexpr int kBorderWidth = 3;
  static constexpr int kAnimeDur = 300;

  Highlight highlight_;
  // 动画此刻正在显示的锚点框（anime_ 逐帧写它；动画停着时等于
  // highlight_.anchor）
  QRect shown_anchor_;
  QPixmap pixmap_;
  QVariantAnimation* anime_;
};

#endif  // FILE_TOUR_OVERLAY_MASK_H_
