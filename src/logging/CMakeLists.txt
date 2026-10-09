# 日志核心：**不依赖 Qt**，只链 spdlog 与标准库
#
# 不链 Qt 的收益和 hal / measure 一样：这一层的单测不需要 Qt 在 PATH 上、
# 不需要插件环境，exe 能单独运行。
#
# 不做成动态库：spdlog 在本仓是静态库（3rdparty/spdlog-1.17.0/CMakeLists.txt:184），
# 而除本模块外还有别处在直接用 spdlog（如 core/crashhandler 的
# spdlog::default_logger()->flush()）。做成 DLL 会让进程里出现两份 spdlog
# 注册表，崩溃路径上 flush 到的就是另一个空 logger。
set(TARGET_NAME ${APP_TARGET_PREFIX}_log)

set(ALL_HEADERS_SOURCES
    log_types.h

    log_pattern.cpp
    log_pattern.h

    log_core.cpp
    log_core.h
)

add_library(${TARGET_NAME} STATIC ${ALL_HEADERS_SOURCES})

# 暴露 src，消费方用 #include "logging/log_core.h"
target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# **PUBLIC 是必须的，不是随手写的**：spdlog 的 SPDLOG_COMPILED_LIB 靠 PUBLIC
# 传播（3rdparty/spdlog-1.17.0/CMakeLists.txt:194）。若某个消费方拿到了
# log_core.h 却没链到 spdlog::spdlog，它会按 header-only 编一份自己的 spdlog
# —— 那才是真正会出问题的"两份注册表"
target_link_libraries(${TARGET_NAME} PUBLIC
    spdlog::spdlog
)

# 没有 AUTOMOC：本层没有 QObject 派生类
