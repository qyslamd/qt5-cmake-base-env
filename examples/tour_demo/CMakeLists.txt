set(TARGET_NAME "tour_demo")

set(ALL_SRCS
    main.cpp
    demo_widget.cpp
    demo_widget.h
    demo_topics.cpp
    demo_topics.h
)

add_executable(${TARGET_NAME} WIN32 ${ALL_SRCS})

target_link_libraries(${TARGET_NAME} PRIVATE
    ${APP_TARGET_PREFIX}_tour
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOUIC ON
    AUTOMOC ON
    AUTORCC ON
)

# 与库保持一致：MSVC 默认只有 /W3
if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)
endif()

# 生成运行脚本（Windows 为 .bat，Linux 为 .sh），输出到可执行文件所在目录
configure_run_script("run_${TARGET_NAME}")
