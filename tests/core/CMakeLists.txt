# 崩溃捕获与异常处理
#
# 本测试只覆盖自研方案（CrashHandler）：子进程拉起 → 触发 SEH/信号 →
# 断言 crash_*.log 落盘与内容结构。
#
# APP_ENABLE_CRASHPAD=ON 时 core 里没有 CrashHandler 符号（改用
# crashpad_bootstrap），此测试编译不过，故整体跳过。
# Crashpad 侧的集成测试需要另写：拉起 crashpad_handler，
# 断言 %LOCALAPPDATA%/.../crash/*.dmp 落盘——那是独立的一步，
# 不在本文件覆盖范围。
if(NOT APP_ENABLE_CRASHPAD)
    add_unit_test(
        NAME test_core_crashhandler
        SOURCES crashhandler_test.cpp
        LIBS ${APP_TARGET_PREFIX}_core
        QT_MODULES Core Test
        LABELS CORE
        NO_GTEST_MAIN
    )
endif()