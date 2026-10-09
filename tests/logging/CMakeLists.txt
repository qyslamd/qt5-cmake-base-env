# 日志核心的单测。**不带 QT_MODULES**：ephad_log 不依赖 Qt，这个 exe 也就不链
# Qt、不需要 Qt 在 PATH 上——这本身就是"核心真的与 Qt 无关"的证据
add_unit_test(
    NAME test_log_core
    SOURCES log_core_test.cpp
    LIBS ${APP_TARGET_PREFIX}_log
    LABELS LOG
)
