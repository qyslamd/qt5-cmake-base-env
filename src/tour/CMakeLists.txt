set(TARGET_NAME ${APP_TARGET_PREFIX}_tour)

set(TARGET_SRCS
    tour_bridge.cpp
    tour_bridge.h
    tour_bubble_placer.cpp
    tour_bubble_placer.h
    tour_bubble_view.cpp
    tour_bubble_view.h
    tour_catalog.cpp
    tour_catalog.h
    tour_colors.h
    tour_controller.cpp
    tour_controller.h
    tour_countdown.cpp
    tour_countdown.h
    tour_homepage.cpp
    tour_homepage.h
    tour_mask_geometry.cpp
    tour_mask_geometry.h
    tour_menu.cpp
    tour_menu.h
    tour_overlay.cpp
    tour_overlay.h
    tour_overlay_mask.cpp
    tour_overlay_mask.h
    tour_style.cpp
    tour_style.h
)

# 资源用 qt5_add_resources 显式生成，不同时开 AUTORCC——两套机制并存时看不出
# 哪套在生效（实测只有前者产出了目标文件）。静态库里的 qrc 符号不会被自动
# 链接进来，由 tour_style.cpp 的 Q_INIT_RESOURCE 显式引用
set(TOUR_RESOURCES tour_res.qrc)
qt5_add_resources(TOUR_RESOURCES_CPP ${TOUR_RESOURCES})

add_library(${TARGET_NAME} STATIC ${TARGET_SRCS} ${TOUR_RESOURCES_CPP})

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
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
