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

# 宏归一口径：fluent 才有宏（APP_UI_FLUENT），qss 是缺省无宏。本示例的 fluent
# 分支是空操作（维持系统默认样式），只注宏、不链 FluentUI3Style；qss 缺省路
# 链底座样式表（src/style）——示例没有自己的项目层，看到的就是纯底座层
if(APP_UI_ROUTE STREQUAL "fluent")
    target_compile_definitions(${TARGET_NAME} PRIVATE APP_UI_FLUENT)
else()
    target_link_libraries(${TARGET_NAME} PRIVATE ${APP_TARGET_PREFIX}_style)
endif()

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
