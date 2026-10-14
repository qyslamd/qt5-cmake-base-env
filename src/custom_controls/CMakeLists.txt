set(TARGET_NAME ${APP_TARGET_PREFIX}_controls)

set(TARGET_SRCS
    switch_button.cpp
    switch_button.h
    hover_button.cpp
    hover_button.h
    password_eye_button.cpp
    password_eye_button.h
)

set(CONTROL_RESOURCES custom_control_res.qrc)
qt5_add_resources(CONTROL_RESOURCES_CPP ${CONTROL_RESOURCES})

add_library(${TARGET_NAME} STATIC ${TARGET_SRCS} ${CONTROL_RESOURCES_CPP})

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
