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
// 从 TourOverlay 里抽出来的——"高亮在哪儿"原先在那边存了三份（锚点矩形、
// 额外靶表、动画副本），现在只有这里一份：Highlight 是**要去的地方**，
// shown_anchor_ 是**此刻显示中的框**（动画的输出，不是第二份真相）。
//
// 它只管"画什么"与"此刻框在哪儿"：谁是靶、要不要动画由 TourOverlay 定；
// 鼠标落在靶上没有也问它（点在靶上不算点空白，见 hitTarget）。
class OverlayMask : public QObject {
  Q_OBJECT

 public:
  explicit OverlayMask(QObject* parent = nullptr);

  // 换高亮。animated=true 时从"此刻显示中的框"挪过去（单靶那种）；给了
  // extras 的一步不走动画（设计 D2：并集难看、N 路并行不值），直接对齐
  void setHighlight(const QRect& anchor, const QList<QRect>& extras,
                    bool animated);
  // 图示步那张图（贴在锚点那块上）。紧随其后总是 setHighlight
  void setPixmap(const QPixmap& pixmap);
  void clear();

  // 锚点（为空 = 纯讲解步：气泡居中、不画箭头）
  QRect anchor() const { return highlight_.anchor; }
  // 一共框了几处（含锚点）。空锚点不算——那一步屏幕上本来就没有框
  int count() const { return highlight_.count(); }

  // 点在靶上？**逐个矩形判，不合并并集**（设计 D3）：多靶时两块之间那块空白
  // 也算"点在目标上"的话，遮罩就永远关不掉了（讲解式主题点空白 = 退出）
  bool hitTarget(const QPoint& pos) const;

  // 画整块遮罩。viewport 是遮罩自己的矩形——本类不持有控件
  void paint(QPainter* painter, const QRect& viewport) const;

 signals:
  // 该重画了（换高亮、清空、动画的每一帧）
  void repaintRequested();

 private:
  // 当前这一步要高亮的框：**一处真相**。原先分成"锚点矩形 + 额外靶表"两份，
  // 再叠一份动画副本，靠约定同步——于是有幽灵框（多靶那条路忘了同步副本）、
  // 有"清完之后 extra 必然为空"这种恒真条件，还要在注释里反复提醒"别拿派生
  // 状态当判据"（那几条注释现在可以删了）
  struct Highlight {
    // 锚点：气泡与三角箭头挂在它旁边。**可以为空**（空目标步是纯讲解），
    // 但位置是定的——它是第一项
    QRect anchor;
    // 其余要一起框住的靶（空矩形不进来，见 TourOverlay::updateUi 的收集循环）
    QList<QRect> extras;

    // 只留真有人用的那个：count() 给 highlightCount()
    int count() const { return (anchor.isNull() ? 0 : 1) + extras.size(); }
  };

  // 命中与绘制共用的矩形表：**显示中的**锚点 + 额外靶
  QList<QRect> TargetRects() const;

  Highlight highlight_;
  // 动画此刻正在显示的锚点框（anime_ 逐帧写它）。它不是第二份真相：动画
  // 停着时它等于 highlight_.anchor
  QRect shown_anchor_;
  QPixmap pixmap_;
  // 洞比靶往外扩这么多（框线也画在这一圈上）
  const int margin_ = 10;
  const int borderWidth_ = 3;
  const int kAnimeDur = 300;
  QVariantAnimation* anime_;
};

#endif  // FILE_TOUR_OVERLAY_MASK_H_
