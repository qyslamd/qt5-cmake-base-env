#ifndef FILE_TOUR_BUBBLE_PLACER_H_
#define FILE_TOUR_BUBBLE_PLACER_H_

#include <QPoint>
#include <QRect>
#include <QSize>

// 气泡摆哪儿：**纯几何，与控件无关**，所以能逐分支单测（tests/tour/）。
//
// 坐标一律是遮罩那一套（左上为原点）：调用方负责把全局矩形映射进来，再把
// 结果换算到气泡自己的坐标（见 TourBubbleView::followAnchor）
enum class BubbleSide {
  kLeft,     // 气泡在目标左侧
  kRight,    // 右侧
  kTop,      // 上方
  kBottom,   // 下方
  kCentred,  // 没有目标可依托（空锚点）：画面居中，**不改箭头方向**
};

struct BubblePlacement {
  QPoint top_left;  // 气泡左上角（遮罩坐标系）
  BubbleSide side;
};

// 钳位时离视口边留的缝。**调用方要保证视口比气泡大出两个它**，否则四向都放
// 不下、只能退回"挑最大的一侧 + 钳位"，气泡就被按到目标上了（见
// tour_mask_geometry.h：遮罩撑到多大才够）
constexpr int kBubbleClampMargin = 20;

// anchor 为空 → 居中；否则按四向可用空间挑一侧，再钳进视口。
//
// `gap` 是气泡与目标之间留的缝，**由调用方给**：它跟着布局边距走，写死在这里
// 就会与布局脱钩。判定缝是这一处自己的口径，见 .cpp
BubblePlacement PlaceBubble(const QRect& anchor, const QSize& bubble,
                            const QSize& viewport, int gap);

#endif  // FILE_TOUR_BUBBLE_PLACER_H_
