# 崩溃捕获与异常处理
add_unit_test(
    NAME test_core_crashhandler
    SOURCES crashhandler_test.cpp
    LIBS ${APP_TARGET_PREFIX}_core
    QT_MODULES Core Test
    LABELS CORE
    # 测试自提供 main：需在 gtest 初始化之前拦截「崩溃子进程」模式参数
    NO_GTEST_MAIN
)
