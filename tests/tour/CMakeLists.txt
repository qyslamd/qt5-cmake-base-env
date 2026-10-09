# tour 库的第一批单测。
#
# 之前这个库一条单测都没有——它的判据只有分支上那几条 funq UI 用例（要拉起
# 整个产品、注入、点界面），改一处几何就得跑几十秒。气泡定位是纯几何，抽成
# tour_bubble_placer 之后有了**毫秒级、可逐分支断言**的判据。
#
# 被测的 placer 不碰控件，但 tour 这个静态库是链 Qt 的（Gui/Widgets），所以
# 这里得带 QT_MODULES
add_unit_test(
    NAME test_tour_bubble_placer
    SOURCES bubble_placer_test.cpp
    LIBS ${APP_TARGET_PREFIX}_tour
    QT_MODULES Core Gui Widgets
    LABELS TOUR
)

# 倒计时的判据（TourCountdown）：那条"静默趟只显示、不推进"的契约。它是
# QObject + QTimer，不碰控件，所以一条 1 秒的静默趟就能把两条边都走到。
# 要自己起 QCoreApplication（走事件循环），所以是 NO_GTEST_MAIN
add_unit_test(
    NAME test_tour_countdown
    SOURCES countdown_test.cpp
    LIBS ${APP_TARGET_PREFIX}_tour
    QT_MODULES Core
    LABELS TOUR
    NO_GTEST_MAIN
)

# 遮罩铺法（PlaceMask）：宿主比"最小视口"小时撑大、再裁到屏幕。
# 症状是"登录框上的气泡压着目标"，而这段逻辑原先埋在 setHost 里、只对
# inflate 那条路生效——纯几何才是能逐情形断言的地方
add_unit_test(
    NAME test_tour_mask_geometry
    SOURCES mask_geometry_test.cpp
    LIBS ${APP_TARGET_PREFIX}_tour
    QT_MODULES Core
    LABELS TOUR
)
