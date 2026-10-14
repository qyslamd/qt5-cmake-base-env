set(TARGET_NAME ${APP_TARGET_PREFIX}_style)

set(TARGET_SRCS
    style_sheet.cpp
    style_sheet.h
)

# 资源用 qt5_add_resources 显式生成，不同时开 AUTORCC——两套机制并存时看不出
# 哪套在生效（实测只有前者产出了目标文件）。静态库里的 qrc 符号不会被自动
# 链接进来，由 style_sheet.cpp 的 Q_INIT_RESOURCE 显式引用
set(STYLE_RESOURCES style_res.qrc)
qt5_add_resources(STYLE_RESOURCES_CPP ${STYLE_RESOURCES})

add_library(${TARGET_NAME} STATIC ${TARGET_SRCS} ${STYLE_RESOURCES_CPP})

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
)

# PUBLIC：对外头文件用到 Qt 类型，宿主需要连着这些依赖一起拿到
target_link_libraries(${TARGET_NAME} PUBLIC
    Qt5::Gui
    Qt5::Widgets
    Qt5::Core
)

target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)
endif()
