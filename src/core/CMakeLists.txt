set(TARGET_NAME ${APP_TARGET_PREFIX}_core)

set(ALL_HEADERS_SOURCES
    config/config_manager.cpp
    logger/log_filter.cpp
    logger/logger.cpp
    logger/log_history_buffer.cpp
    logger/qt_console_sink.cpp
    config/config_defs.h
    config/config_manager.h
    logger/log_filter.h
    logger/logger.h
    logger/log_history_buffer.h
    logger/qt_console_sink.h
    common/single_instance.cpp
    common/single_instance.h

    # 统一入口：三态后端由 APP_CRASH_BACKEND 切换，本文件三种都编
    crashhandler/crash_bootstrap.cpp
    crashhandler/crash_bootstrap.h
    # 信号兜底 + Qt 消息重定向。它管的不只是崩溃，但崩溃信号那半与各后端的
    # 职责划分有关，所以放在这个目录（见文件头注释）
    crashhandler/global_exception_handler.cpp
    crashhandler/global_exception_handler.h
)

# 登录认证：APP_ENABLE_AUTH=OFF 时整块裁掉（见顶层 CMakeLists 的说明）
if(APP_ENABLE_AUTH)
    list(APPEND ALL_HEADERS_SOURCES
        auth/auth_service.cpp
        auth/auth_service.h
        auth/auth_types.h
        auth/password_hasher.cpp
        auth/password_hasher.h
        auth/user_manager.cpp
        auth/user_manager.h
    )
endif()

# 崩溃捕获实现三选一。APP_ENABLE_CRASHPAD / APP_ENABLE_CRASH_CAPTURE 由顶层
# CMakeLists 从 APP_CRASH_BACKEND 派生，子目录里可直接读（none 时两个都为 OFF）
if(APP_ENABLE_CRASHPAD)
    list(APPEND ALL_HEADERS_SOURCES
        crashhandler/crashpad_bootstrap.cpp
        crashhandler/crashpad_bootstrap.h
    )
elseif(APP_ENABLE_CRASH_CAPTURE)
    list(APPEND ALL_HEADERS_SOURCES
        crashhandler/crash_handler.cpp
        crashhandler/crash_handler.h
    )
    # 平台实现按平台加入（源文件自身亦有平台守卫，此处为防御性组织）
    if(WIN32)
        list(APPEND ALL_HEADERS_SOURCES
            crashhandler/windows_crash_handler.cpp
            crashhandler/windows_crash_handler.h
        )
    else()
        list(APPEND ALL_HEADERS_SOURCES
            crashhandler/linux_crash_handler.cpp
            crashhandler/linux_crash_handler.h
        )
    endif()
endif()

# ============================
# 添加静态库目标
add_library(${TARGET_NAME} STATIC ${ALL_HEADERS_SOURCES})

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOUIC ON
    AUTOMOC ON
    AUTORCC ON
)

# 暴露 src/core 作为包含目录，消费方可用 #include "logger/logger.h"
target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# logger 目录现在只剩 Qt 桥（门面 + sink + 历史缓冲 + 过滤器）；
# 核心在 ephad_log 里，它才是链 spdlog 的那个
target_link_libraries(${TARGET_NAME} PUBLIC
    Qt5::Core
    Qt5::Network
    ${APP_TARGET_PREFIX}_log
)

# 崩溃捕获的链接依赖，随开关切换
if(APP_ENABLE_CRASHPAD)
    # Crashpad 头文件（客户端 API + mini_chromium 基础类型）
    target_include_directories(${TARGET_NAME} PUBLIC
        "${CMAKE_SOURCE_DIR}/3rdparty/crashpad-7b9686b"
        "${CMAKE_SOURCE_DIR}/3rdparty/mini_chromium-9cdc2a7"
    )

    # Crashpad 静态库。顺序无所谓，CMake 会解析依赖关系；
    # ZLIB::ZLIB 是 crashpad 的强依赖（util 里解压用）
    target_link_libraries(${TARGET_NAME} PUBLIC
        crashpad_client
        crashpad_util
        crashpad_compat
        crashpad_minidump
        crashpad_snapshot
        crashpad_tools
        minichromium
        ZLIB::ZLIB
    )

    if(WIN32)
        target_link_libraries(${TARGET_NAME} PRIVATE
            advapi32 winhttp version user32 PowrProf
        )
    endif()
elseif(APP_ENABLE_CRASH_CAPTURE)
    # 自研方案需要 dbghelp 做符号解析与 minidump 写入
    if(WIN32)
        target_link_libraries(${TARGET_NAME} PRIVATE dbghelp)
    endif()
endif()
# none：不链接任何崩溃相关库

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)
endif()