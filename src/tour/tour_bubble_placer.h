#ifndef FILE_TOUR_BUBBLE_PLACER_H_
#define FILE_TOUR_BUBBLE_PLACER_H_

#include <QPoint>
#include <QRect>
#include <QSize>

// 气泡摆哪儿：**纯几何，与控件无关**。
//
// 从 TourOverlay::changeBubblePosition 里抽出来的（原先是 157 行的分支 +
// 钳位，只能跑起来点一遍才知道对不对）。抽出来的两个理由：
//   1. 它是"给三个矩形/尺寸出两个数"的计算，不该和绘制、动画、鼠标搅在一起
//   2. 抽出来就能**逐分支断言**——本仓 tour 的第一批单测（tests/tour/）
//
// 坐标一律是**遮罩自己那一套**（左上为原点）：调用方负责把全局矩形映射进来、
// 再把结果加上气泡原点（见 TourOverlay::bubbleOrigin）
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

// anchor 为空 → 居中；否则按四向可用空间挑一侧，再钳进视口。
//
// `gap` 是气泡与目标之间留的缝，**由调用方给**：它跟着布局边距走
// （TourOverlay 传 kLayoutMargin - 15），写死在这里就会与布局脱钩。
// 另外两个数（canPlace 的判定缝、钳位边距）是这一处自己的口径，见 .cpp
BubblePlacement PlaceBubble(const QRect& anchor, const QSize& bubble,
                            const QSize& viewport, int gap);

#endif  // FILE_TOUR_BUBBLE_PLACER_H_
