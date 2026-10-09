#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include "tour_countdown.h"

// 要 QCoreApplication：Timer 与事件循环都靠它建出事件分发器。没有它时
// QEventLoop::exec() 建不出分发器，用例会直接卡死（不是失败，是挂住）
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

// 倒计时的判据。这个类的全部理由就是**静默那一趟不推进任何东西**——外部驱动
// 的"马上要切页了"只是个显示，下一步做什么由脚本决定。原先这条约定埋在
// TourOverlay 的 tick lambda 里（靠 countdown_silent_ 那个布尔，走到头还得分
// 两支），抽出来之后它是一条能断言的契约。
//
// 自动挡那 5 秒不在这里等：真等要 5 秒，而它走到头只是 emit finished()，
// 这个等待买不到更多信息。

namespace {

// 转事件循环等 ms 毫秒：QTimer 要有事件循环才走。本用例只用 QtCore，不引 QTest
void WaitMs(int ms) {
  QEventLoop loop;
  QTimer::singleShot(ms, &loop, &QEventLoop::quit);
  loop.exec();
}

}  // namespace

// 起表那一帧显示的就是传入的秒数——界面上那个数字由它给
TEST(TourCountdownTest, SilentTripShowsTheGivenSeconds) {
  TourCountdown countdown;
  countdown.startSilent(3);
  EXPECT_TRUE(countdown.showing());
  EXPECT_EQ(3, countdown.remaining());
  EXPECT_DOUBLE_EQ(0.0, countdown.elapsed());  // 0.0 = 刚开始（条子满）
  countdown.stop();
}

// 静默那一趟走到头：显示收掉、条子走空，但**不发 finished()**
TEST(TourCountdownTest, SilentTripEndsWithoutAdvancing) {
  TourCountdown countdown;
  bool advanced = false;
  QObject::connect(&countdown, &TourCountdown::finished,
                   [&advanced] { advanced = true; });

  // 1 秒那一趟 = 1000 / 30 = 33 个 tick
  countdown.startSilent(1);
  WaitMs(1200);

  EXPECT_FALSE(countdown.showing());
  EXPECT_DOUBLE_EQ(1.0, countdown.elapsed());
  EXPECT_FALSE(advanced);
}

// 自动挡开关同时决定"该不该露脸"（原先这是 setIsAutoMode 里单独写的一句
// setVisible，与绘制那条守卫各判各的）
TEST(TourCountdownTest, AutoModeDrivesTheDisplay) {
  TourCountdown countdown;
  EXPECT_FALSE(countdown.showing());

  countdown.setAutoMode(true);
  EXPECT_TRUE(countdown.showing());

  countdown.setAutoMode(false);
  EXPECT_FALSE(countdown.showing());
}

// stop() 只是"别再走"：显示不动（点了按钮，窗口里的秒数不该凭空消失）
TEST(TourCountdownTest, StopKeepsTheDisplay) {
  TourCountdown countdown;
  countdown.startSilent(3);
  countdown.stop();
  EXPECT_TRUE(countdown.showing());
  EXPECT_EQ(3, countdown.remaining());
}
