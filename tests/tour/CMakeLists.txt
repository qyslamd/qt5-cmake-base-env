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
