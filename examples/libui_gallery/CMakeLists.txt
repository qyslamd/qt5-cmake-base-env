set(TARGET_NAME "libui_gallery")

set(ALL_SRC
    main.cpp
    gallery_window.cpp
    gallery_window.h
)

# 日志面板位于 app 层：它依赖 core 的 QtConsoleSink 与 LogHistoryBuffer，
# 而 libui 刻意不依赖 core（以便整包复用到其它工程），故本示例直接编入
# 这两个源文件，而不是把面板塞进 libui。
set(APP_LOG_UI_SRC
    ${CMAKE_SOURCE_DIR}/src/app/log_output_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/app/log_output_panel.h
    ${CMAKE_SOURCE_DIR}/src/app/log_filter_bar.cpp
    ${CMAKE_SOURCE_DIR}/src/app/log_filter_bar.h
)

add_executable(${TARGET_NAME} WIN32)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

configure_run_script("run_${TARGET_NAME}")

target_sources(${TARGET_NAME} PRIVATE ${ALL_SRC} ${APP_LOG_UI_SRC})

target_include_directories(${TARGET_NAME} PRIVATE
    # 上述日志面板源文件按 "log_filter_bar.h" 这类同目录写法包含彼此
    ${CMAKE_SOURCE_DIR}/src/app
)

# libui 与 core 都对外公开头文件目录，故可直接 #include "sector_menu.h"
# 与 "logger/logger.h"
target_link_libraries(${TARGET_NAME}
    PRIVATE
        ${APP_TARGET_PREFIX}_libui
        ${APP_TARGET_PREFIX}_core
        Qt5::Widgets
        Qt5::Core
        Qt5::Gui
)
