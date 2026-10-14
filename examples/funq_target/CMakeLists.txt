set(TARGET_NAME "funq_target")

set(ALL_SRCS
    main.cpp
    funq_target_window.cpp
    funq_target_window.h
)

add_executable(${TARGET_NAME} WIN32 ${ALL_SRCS})

# 只链 Qt Widgets：靶子要「控件少而稳、依赖越薄越好」——它一旦链上本仓的
# 业务模块，测试的前置条件就会跟着那些模块的开关走（样式底座不算业务模块：
# 没有开关、也不拉业务依赖，见下面那段）
target_link_libraries(${TARGET_NAME} PRIVATE
    Qt5::Widgets
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
    AUTOMOC ON
)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)
endif()

configure_run_script("run_${TARGET_NAME}")
