set(TARGET_NAME ${APP_TARGET_PREFIX}_libui)

set(ALL_HEADERS_SOURCES
    switch_button.cpp
    switch_button.h
    splash_screen.cpp
    splash_screen.h
    radial_navigator.cpp
    radial_navigator.h
    sector_menu.cpp
    sector_menu.h
)

add_library(${TARGET_NAME} STATIC ${ALL_HEADERS_SOURCES})

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

# 只依赖 Qt Widgets：本库放的是与业务无关的通用 UI 部件，
# 不引入 core，以便整包复用到其它工程
target_link_libraries(${TARGET_NAME} PUBLIC
    Qt5::Widgets
)

# 暴露本目录作为包含目录，消费方可用 #include "switch_button.h"
target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)
endif()
