#include <gtest/gtest.h>

#include <QRect>
#include <QSize>

#include "tour_mask_geometry.h"

// 遮罩铺法的判据：**逐情形**。这段逻辑原先埋在 TourOverlay::setHost 里、只对
// inflate 那条路生效，症状是"登录框（460×370）上的气泡压着目标"——四向都放不
// 下时 PlaceBubble 只能退回"挑最大侧 + 钳位"。
//
// 期望值手算，算式写在每条断言上方（同 bubble_placer_test）
//
// 一条口径：**QRect::center() 是 (x1+x2)/2 向下取整**，遮罩以宿主中心为中心
// 放大时用的就是它

namespace {

// 屏幕可用区取 1920×1040（够大，专门用来验"放大"这一支）
const QRect kScreen(0, 0, 1920, 1040);

// 登录框：460×370，摆在屏幕中间偏上（x 730..1189，y 300..669）
const QRect kLogin(730, 300, 460, 370);

// 气泡最小 300×200，两侧各一个钳位边距（20）→ 640×440
const QSize kMinViewport(640, 440);

}  // namespace

// 宿主比最小视口大 → **原样返回**（主窗口那条路一点没变）
TEST(MaskGeometryTest, KeepsTheHostWhenItIsBigEnough) {
  const QRect main_window(100, 50, 1024, 900);
  EXPECT_EQ(main_window, PlaceMask(main_window, kMinViewport, kScreen));
}

// 登录框这条：放大到最小视口，且**包住宿主**。
// 460×370 的中心 = ((730+1189)/2, (300+669)/2) = (959, 484)（向下取整）
// → 遮罩左上角 = (959-320, 484-220) = (639, 264)
//
// 不断言"两个中心相等"：向下取整让它差 1 像素（遮罩算出来是 958/483），这不是
// 缺陷——框在放大这一支本来就是动的。要紧的是**包住宿主**
TEST(MaskGeometryTest, GrowsTheMaskAroundTheHostCentre) {
  const QRect mask = PlaceMask(kLogin, kMinViewport, kScreen);
  EXPECT_EQ(QRect(639, 264, 640, 440), mask);
  EXPECT_TRUE(mask.contains(kLogin));
}

// 宿主贴屏幕右上角：放大之后按屏幕裁掉上边与右边，**可见部分仍包住宿主**。
// 中心 (1889, 19) → 左上角 (1889-320, 19-220) = (1569, -201) → 裁完
// x 1569..1919（屏幕右边界）、y 0..238
TEST(MaskGeometryTest, ClipsToTheScreenWhenTheHostHugsTheEdge) {
  const QRect host(1860, 0, 60, 40);  // 右上角一小块
  const QRect mask = PlaceMask(host, kMinViewport, kScreen);
  EXPECT_EQ(QRect(1569, 0, 351, 239), mask);
  EXPECT_TRUE(mask.contains(host));
}

// 屏幕比最小视口还小（小屏/远程桌面）：结果就是整块屏幕——再大也没地方放，
// 退化路径（PlaceBubble 的钳位）接着兜底
TEST(MaskGeometryTest, FallsBackToTheScreenWhenItIsTooSmall) {
  const QRect tiny_screen(0, 0, 400, 300);
  const QRect host(50, 40, 200, 150);
  EXPECT_EQ(tiny_screen, PlaceMask(host, kMinViewport, tiny_screen));
}

// 没有屏幕信息（理论上的分支）时只放大、不裁剪
TEST(MaskGeometryTest, GrowsWithoutClippingWhenScreenIsUnknown) {
  EXPECT_EQ(QRect(639, 264, 640, 440),
            PlaceMask(kLogin, kMinViewport, QRect()));
}

// 空宿主：原样返回空（调用方在此之前已经挡了 host == nullptr）
TEST(MaskGeometryTest, EmptyHostStaysEmpty) {
  EXPECT_TRUE(PlaceMask(QRect(), kMinViewport, kScreen).isNull());
}
