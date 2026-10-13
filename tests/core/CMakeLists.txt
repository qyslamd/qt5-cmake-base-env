# 日志系统：启动期（init 之前）与之后的行为
#
# 与崩溃后端无关，所以放在最外层无条件注册
add_unit_test(
    NAME test_core_logger
    SOURCES logger_test.cpp
    LIBS ${APP_TARGET_PREFIX}_core
    QT_MODULES Core
    LABELS CORE
    NO_GTEST_MAIN
)

# 崩溃捕获与异常处理：三个后端常编（见 crash_bootstrap.h），两个测试都无条件注册
#
# 自研方案（CrashHandler）：子进程拉起 → 触发 SEH/信号 → 断言 crash_*.log
# 落盘与内容结构。
add_unit_test(
    NAME test_core_crashhandler
    SOURCES crashhandler_test.cpp
    LIBS ${APP_TARGET_PREFIX}_core
    QT_MODULES Core Test
    LABELS CORE
    NO_GTEST_MAIN
)

# Crashpad 报告导出：造一份 pending 报告 → 断言导出落盘与交付语义，
# 不需要真崩溃，也不需要 handler。「真崩一次 → dump 落盘」的端到端验证在
# 演练里做（见 docs/plan/plan总结.md 的「崩溃报告回传链路」），不进单元测试：
# 它要拉起 handler，且结果依赖进程外进程的时序
add_unit_test(
    NAME test_core_crash_report
    SOURCES crash_report_test.cpp
    LIBS ${APP_TARGET_PREFIX}_core
    QT_MODULES Core Test
    LABELS CORE
)
