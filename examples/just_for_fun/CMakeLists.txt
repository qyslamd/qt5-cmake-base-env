# 半色调终端艺术示例（ascii.rest 的 Qt 直译）
set(TARGET_NAME "just_for_fun")

add_executable(${TARGET_NAME} WIN32
    just_for_fun.qrc
    main.cpp
    widget.h
    widget.cpp
    ascii_dot_view.h
    ascii_gl_view.h
    marine_drive.h
    marine_drive.cpp
    alpine_dawn.h
    alpine_dawn.cpp
    desert_night.cpp
    desert_night.h
)

target_link_libraries(${TARGET_NAME} PRIVATE
    ${APP_TARGET_PREFIX}_style
    Qt5::OpenGL
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

# 与库保持一致：MSVC 默认只有 /W3
if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)
else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

configure_run_script("run_${TARGET_NAME}")
