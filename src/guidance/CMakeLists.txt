set(TARGET_NAME ${APP_TARGET_PREFIX}_guidance)

set(TARGET_SRCS
    guidance_config.cpp
    guidance_config.h
    guidance_controller.cpp
    guidance_controller.h
    guidance_homepage.cpp
    guidance_homepage.h
    guidance_presentation.cpp
    guidance_presentation.h
    guidance_style.cpp
    guidance_style.h
    guidance_support.cpp
    guidance_support.h
)

# 资源用 qt5_add_resources 显式生成，不同时开 AUTORCC——两套机制并存时看不出
# 哪套在生效（实测只有前者产出了目标文件）。静态库里的 qrc 符号不会被自动
# 链接进来，由 guidance_style.cpp 的 Q_INIT_RESOURCE 显式引用
set(GUIDANCE_RESOURCES guidance_res.qrc)
qt5_add_resources(GUIDANCE_RESOURCES_CPP ${GUIDANCE_RESOURCES})

add_library(${TARGET_NAME} STATIC ${TARGET_SRCS} ${GUIDANCE_RESOURCES_CPP})

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
