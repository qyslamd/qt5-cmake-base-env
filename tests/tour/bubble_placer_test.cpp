#include <gtest/gtest.h>

#include <QPoint>
#include <QRect>
#include <QSize>

#include "tour_bubble_placer.h"

// 气泡定位的判据：**逐分支**。原先这段是 TourOverlay 里 157 行的 if/else 与
// 钳位，只能把产品拉起来点一遍才知道对不对；现在它是纯函数，一个一个分支断。
//
// 期望值都是手算出来的，算式写在每条断言上方——**别从实现反推期望值**，
// 那样这段测试就成了实现的镜像，实现错它也错。
//
// 一条口径先记在这里：**`QRect::center()` 是 `(x1 + x2) / 2`，向下取整**，
// 不是几何中心（宽 200 的矩形，400..599 的中心报 499 而不是 500）。摆气泡
// 用的是它，所以下面凡是按"目标中线"算的期望值都比几何中线小 1

namespace {

// 常用的一组尺寸：视口 1000x800、气泡 200x100、缝 11
// （11 = TourOverlay 的 kLayoutMargin 26 - 15，与产品里的口径一致）
constexpr int kViewportW = 1000;
constexpr int kViewportH = 800;
const QSize kBubble(200, 100);
constexpr int kGap = 11;

const QSize kViewport(kViewportW, kViewportH);

}  // namespace

// 空锚点（纯讲解步）→ 视口居中。
// 居中 = ((1000-200)/2, (800-100)/2) = (400, 350)；这一支**不带方向**
TEST(BubblePlacerTest, EmptyAnchorIsCentred) {
  const BubblePlacement placed = PlaceBubble(QRect(), kBubble, kViewport, kGap);
  EXPECT_EQ(QPoint(400, 350), placed.top_left);
  EXPECT_EQ(BubbleSide::kCentred, placed.side);
}

// 两侧都放得下 → 挑**空间更大**的那侧。这里左边 400、右边 401，右边赢。
// x = 右边界的右侧 + 缝 = 599 + 11 = 610；y = 目标中线 - 气泡半高 = 400 - 50
TEST(BubblePlacerTest, PicksTheRoomierSide) {
  const QRect anchor(400, 300, 200, 200);
  const BubblePlacement placed = PlaceBubble(anchor, kBubble, kViewport, kGap);
  EXPECT_EQ(QPoint(610, 349), placed.top_left);
  EXPECT_EQ(BubbleSide::kRight, placed.side);
}

// 目标贴着左边（左侧 100 < 气泡 200 + 缝 15 = 215）→ 只能去右边。
// x = 299 + 11 = 310，y = center().y()(499) - 50 = 449 ... 见上面那条口径：
// 300..499 的中线报 399，故 y = 399 - 50 = 349
TEST(BubblePlacerTest, LeftSideTooNarrowGoesRight) {
  const QRect anchor(100, 300, 200, 200);
  const BubblePlacement placed = PlaceBubble(anchor, kBubble, kViewport, kGap);
  EXPECT_EQ(QPoint(310, 349), placed.top_left);
  EXPECT_EQ(BubbleSide::kRight, placed.side);
}

// 目标贴着右边（右侧只剩 11 < 215）→ 只能去左边。
// x = 目标左边界 - 气泡宽 - 缝 = 800 - 200 - 11 = 589，y = 350
TEST(BubblePlacerTest, RightSideTooNarrowGoesLeft) {
  const QRect anchor(800, 300, 190, 200);
  const BubblePlacement placed = PlaceBubble(anchor, kBubble, kViewport, kGap);
  EXPECT_EQ(QPoint(589, 349), placed.top_left);
  EXPECT_EQ(BubbleSide::kLeft, placed.side);
}

// 横着两边都放不下（目标宽 900）→ 落到纵向。上方空间 400、下方 301，
// 上方赢。x = 目标中线（499）- 气泡半宽 = 399；
// y = 目标上边界 - 气泡高 - 缝 = 400 - 100 - 11 = 289
TEST(BubblePlacerTest, FallsBackToVerticalWhenNeitherSideFits) {
  const QRect anchor(50, 400, 900, 100);
  const BubblePlacement placed = PlaceBubble(anchor, kBubble, kViewport, kGap);
  EXPECT_EQ(QPoint(399, 289), placed.top_left);
  EXPECT_EQ(BubbleSide::kTop, placed.side);
}

// 纵向上方更宽（下方只剩 100）→ 上方。x = 499 - 100 = 399，
// y = 200 - 100 - 11 = 89
TEST(BubblePlacerTest, VerticalPicksTheRoomierSide) {
  const QRect anchor(50, 200, 900, 500);
  const BubblePlacement placed = PlaceBubble(anchor, kBubble, kViewport, kGap);
  EXPECT_EQ(QPoint(399, 89), placed.top_left);
  EXPECT_EQ(BubbleSide::kTop, placed.side);
}

// 四侧都放不下（气泡比视口还大）→ 挑空间最大的那侧，再**钳进视口**。
// 视口 300x200、气泡 400x300：四侧 canPlace 全假；空间 左 100 / 右 151 /
// 上 50 / 下 101 → 右边最大。算出来的位置是 (160, -75)，钳完
// x = max(20, min(160, 300-400-20)) = max(20, -120) = 20，y 同理 = 20
TEST(BubblePlacerTest, ClampsWhenNothingFits) {
  const QSize tiny_viewport(300, 200);
  const QSize big_bubble(400, 300);
  const QRect anchor(100, 50, 50, 50);
  const BubblePlacement placed =
      PlaceBubble(anchor, big_bubble, tiny_viewport, kGap);
  EXPECT_EQ(QPoint(20, 20), placed.top_left);
  EXPECT_EQ(BubbleSide::kRight, placed.side);
}

// 钳位的另一头：目标在左上角，右侧放得下，但气泡按目标中线摆出来 y = 0，
// 于是被钳到上边距 20。x = 99 + 11 = 110
TEST(BubblePlacerTest, ClampsToTheTopMargin) {
  const QRect anchor(0, 0, 100, 100);
  const BubblePlacement placed = PlaceBubble(anchor, kBubble, kViewport, kGap);
  EXPECT_EQ(QPoint(110, 20), placed.top_left);
  EXPECT_EQ(BubbleSide::kRight, placed.side);
}

// 缝是调用方给的：gap 变大，摆出来的位置跟着挪（写死在算法里就与布局脱钩了）
TEST(BubblePlacerTest, GapComesFromTheCaller) {
  const QRect anchor(400, 300, 200, 200);
  const BubblePlacement placed = PlaceBubble(anchor, kBubble, kViewport, 40);
  EXPECT_EQ(QPoint(639, 349), placed.top_left);  // x: 599 + 40；y 同上
}
