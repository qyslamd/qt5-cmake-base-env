set(TARGET_NAME "new_login_dialog")

set(ALL_SRCS
    main.cpp
    login_window.cpp
    login_window.h
    login_window.qrc
)

add_executable(${TARGET_NAME} WIN32 ${ALL_SRCS})

# 两条路线各自的应用样式：qss 路线 = 底座层（src/style）+ 本示例的项目层
# （login_window.qss，由 ApplyStyleSheet 一并装上）；fluent 路线交给
# FluentUI3Style，不加载任何 QSS。本示例是唯一真用 Fluent 的示例
# （main.cpp 要 new FluentUI3Style），所以 fluent 路线连库一起链
if(APP_UI_ROUTE STREQUAL "fluent")
    target_link_libraries(${TARGET_NAME} PRIVATE FluentUI3Style)
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
