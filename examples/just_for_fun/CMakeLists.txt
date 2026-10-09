# 半色调终端艺术示例（ascii.rest 的 Qt 直译）：marine drive 与 alpine dawn
set(TARGET_NAME "just_for_fun")

add_executable(${TARGET_NAME} WIN32
    main.cpp
    marine_drive.h
    marine_drive.cpp
    alpine_dawn.h
    alpine_dawn.cpp
    ascii_dot_view.h
    ascii_gl_view.h
)

# Qt5::OpenGL 给 --gl 时的 AsciiGLView
target_link_libraries(${TARGET_NAME} PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::OpenGL
)

# 与库保持一致：MSVC 默认只有 /W3
if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)
else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# 生成运行脚本（Windows 为 .bat，Linux 为 .sh），输出到可执行文件所在目录
configure_run_script("run_${TARGET_NAME}")
