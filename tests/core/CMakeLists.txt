# 崩溃捕获与异常处理
#
# 自研方案（CrashHandler）：子进程拉起 → 触发 SEH/信号 → 断言 crash_*.log
# 落盘与内容结构。APP_ENABLE_CRASHPAD=ON 时 core 里没有 CrashHandler 符号
# （改用 crashpad_bootstrap），此测试编译不过，故整体跳过。
#
# Crashpad 侧分两层：
#   - 报告导出（本文件下半段）：造一份 pending 报告 → 断言导出落盘与交付语义，
#     不需要真崩溃，也不需要 handler
#   - 「真崩一次 → dump 落盘」的端到端验证在演练里做（见
#     docs/plan/崩溃报告回传链路设计.md），不进单元测试：它要拉起 handler，
#     且结果依赖进程外进程的时序
if(APP_ENABLE_CRASH_CAPTURE)
    add_unit_test(
        NAME test_core_crashhandler
        SOURCES crashhandler_test.cpp
        LIBS ${APP_TARGET_PREFIX}_core
        QT_MODULES Core Test
        LABELS CORE
        NO_GTEST_MAIN
    )
elseif(APP_ENABLE_CRASHPAD)
    add_unit_test(
        NAME test_core_crash_report
        SOURCES crash_report_test.cpp
        LIBS ${APP_TARGET_PREFIX}_core
        QT_MODULES Core Test
        LABELS CORE
    )
endif()
# APP_CRASH_BACKEND=none 时两个测试都不注册：没有 CrashHandler 符号，也没有报告库
