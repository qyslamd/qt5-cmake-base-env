set(TARGET_NAME "libui_gallery")

set(ALL_SRC
    main.cpp
    gallery_window.cpp
    gallery_window.h
)

add_executable(${TARGET_NAME} WIN32)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

configure_run_script("run_${TARGET_NAME}")

target_sources(${TARGET_NAME} PRIVATE ${ALL_SRC})

# libui 对外公开头文件目录，故此处可直接 #include "sector_menu.h"
target_link_libraries(${TARGET_NAME}
    PRIVATE
        ${APP_TARGET_PREFIX}_libui
        Qt5::Widgets
        Qt5::Core
        Qt5::Gui
)
