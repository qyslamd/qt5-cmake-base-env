#include "tour_bubble_placer.h"

#include <QtGlobal>

namespace {

// 判"这一侧放得下吗"时留的缝。**与 gap 不是一个数**（gap 是摆好之后气泡与目标
// 之间的实际间隙，这里是判定用的保守量），原实现里就是两个数，行为照搬不改
constexpr int kFitGap = 15;

// 钳位边距：算出来的位置再靠边也要留出这么多
constexpr int kClampMargin = 20;

// 某一侧的可用空间与"放得下吗"
struct DirectionInfo {
  int space_width = 0;
  int space_height = 0;
  bool can_place = false;
};

DirectionInfo EvaluateSide(const QRect& anchor, const QSize& bubble,
                           const QSize& viewport, BubbleSide side) {
  DirectionInfo info;
  switch (side) {
    case BubbleSide::kLeft:
      info.space_width = anchor.left();
      info.space_height = viewport.height();
      info.can_place = bubble.height() <= viewport.height() - kFitGap * 2 &&
                       bubble.width() + kFitGap <= anchor.left();
      break;
    case BubbleSide::kRight:
      info.space_width = viewport.width() - anchor.right();
      info.space_height = viewport.height();
      info.can_place =
          bubble.height() <= viewport.height() - kFitGap * 2 &&
          bubble.width() + kFitGap <= viewport.width() - anchor.right();
      break;
    case BubbleSide::kTop:
      info.space_width = viewport.width();
      info.space_height = anchor.top();
      info.can_place = bubble.width() <= viewport.width() - kFitGap * 2 &&
                       bubble.height() + kFitGap <= anchor.top();
      break;
    case BubbleSide::kBottom:
      info.space_width = viewport.width();
      info.space_height = viewport.height() - anchor.bottom();
      info.can_place =
          bubble.width() <= viewport.width() - kFitGap * 2 &&
          bubble.height() + kFitGap <= viewport.height() - anchor.bottom();
      break;
    case BubbleSide::kCentred:
      break;
  }
  return info;
}

BubbleSide PickSide(const DirectionInfo& left, const DirectionInfo& right,
                    const DirectionInfo& top, const DirectionInfo& bottom) {
  // 先横后纵：同一行里挑宽的、同一列里挑高的；四侧都放不下就挑空间最大的
  // （宁可压着目标，也不摆到视口外）
  if (left.can_place && right.can_place) {
    return left.space_width > right.space_width ? BubbleSide::kLeft
                                                : BubbleSide::kRight;
  }
  if (left.can_place) {
    return BubbleSide::kLeft;
  }
  if (right.can_place) {
    return BubbleSide::kRight;
  }
  if (top.can_place && bottom.can_place) {
    return top.space_height > bottom.space_height ? BubbleSide::kTop
                                                  : BubbleSide::kBottom;
  }
  if (top.can_place) {
    return BubbleSide::kTop;
  }
  if (bottom.can_place) {
    return BubbleSide::kBottom;
  }
  const int max_space = qMax(qMax(left.space_width, right.space_width),
                             qMax(top.space_height, bottom.space_height));
  if (max_space == left.space_width) {
    return BubbleSide::kLeft;
  }
  if (max_space == right.space_width) {
    return BubbleSide::kRight;
  }
  if (max_space == top.space_height) {
    return BubbleSide::kTop;
  }
  return BubbleSide::kBottom;
}

QPoint SideOffset(const QRect& anchor, const QSize& bubble, BubbleSide side,
                  int gap) {
  switch (side) {
    case BubbleSide::kLeft:
      return QPoint(anchor.left() - bubble.width() - gap,
                    anchor.center().y() - bubble.height() / 2);
    case BubbleSide::kRight:
      return QPoint(anchor.right() + gap,
                    anchor.center().y() - bubble.height() / 2);
    case BubbleSide::kTop:
      return QPoint(anchor.center().x() - bubble.width() / 2,
                    anchor.top() - bubble.height() - gap);
    case BubbleSide::kBottom:
      return QPoint(anchor.center().x() - bubble.width() / 2,
                    anchor.bottom() + gap);
    case BubbleSide::kCentred:
      break;
  }
  return QPoint();
}

}  // namespace

BubblePlacement PlaceBubble(const QRect& anchor, const QSize& bubble,
                            const QSize& viewport, int gap) {
  if (anchor.isEmpty()) {
    // 没有目标可依托（纯讲解步）：直接居中——走下面的钳位会被按到左上角
    return BubblePlacement{QPoint((viewport.width() - bubble.width()) / 2,
                                  (viewport.height() - bubble.height()) / 2),
                           BubbleSide::kCentred};
  }

  const DirectionInfo left =
      EvaluateSide(anchor, bubble, viewport, BubbleSide::kLeft);
  const DirectionInfo right =
      EvaluateSide(anchor, bubble, viewport, BubbleSide::kRight);
  const DirectionInfo top =
      EvaluateSide(anchor, bubble, viewport, BubbleSide::kTop);
  const DirectionInfo bottom =
      EvaluateSide(anchor, bubble, viewport, BubbleSide::kBottom);
  const BubbleSide side = PickSide(left, right, top, bottom);

  QPoint pos = SideOffset(anchor, bubble, side, gap);
  pos.setX(qMax(kClampMargin, qMin(pos.x(), viewport.width() - bubble.width() -
                                                kClampMargin)));
  pos.setY(
      qMax(kClampMargin,
           qMin(pos.y(), viewport.height() - bubble.height() - kClampMargin)));
  return BubblePlacement{pos, side};
}
