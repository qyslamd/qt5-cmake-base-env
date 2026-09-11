set(TARGET_NAME ${APP_TARGET_PREFIX}_core)

set(ALL_HEADERS_SOURCES
    auth/auth_service.cpp
    auth/password_hasher.cpp
    auth/user_manager.cpp
    config/config_manager.cpp
    logger/log_filter.cpp
    logger/logger.cpp
    logger/log_history_buffer.cpp
    logger/log_pattern_builder.cpp
    logger/qt_console_sink.cpp
    auth/auth_service.h
    auth/auth_types.h
    auth/password_hasher.h
    auth/user_manager.h
    config/config_defs.h
    config/config_manager.h
    logger/log_filter.h
    logger/logger.h
    logger/log_history_buffer.h
    logger/log_pattern_builder.h
    logger/qt_console_sink.h
    crashhandler/crash_handler.cpp
    common/file_exception.cpp
    common/time_exception.cpp
    common/string_exception.cpp
    common/byte_exception.cpp
    common/global_exception_handler.cpp
    common/single_instance.cpp
    common/file_exception.h
    common/time_exception.h
    common/string_exception.h
    common/byte_exception.h
    common/global_exception_handler.h
    common/single_instance.h
)

# 平台崩溃捕获实现按平台加入（源文件自身亦有平台守卫，此处为防御性组织）
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

target_link_libraries(${TARGET_NAME} PUBLIC
    Qt5::Core
    Qt5::Network
    spdlog::spdlog
)

if(WIN32)
    target_link_libraries(${TARGET_NAME} PRIVATE dbghelp)
endif()

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)
endif()