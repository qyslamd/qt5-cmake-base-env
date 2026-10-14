set(TARGET_NAME ${APP_TARGET_PREFIX}_markdown)

set(TARGET_SRCS
    code_highlighter.cpp
    code_highlighter.h
    markdown_to_html.cpp
    markdown_to_html.h
    markdown_view.cpp
    markdown_view.h
    markdown_window.cpp
    markdown_window.h
)

set(MARKDOWN_RESOURCES markdown_res.qrc)
qt5_add_resources(MARKDOWN_RESOURCES_CPP ${MARKDOWN_RESOURCES})

add_library(${TARGET_NAME} STATIC ${TARGET_SRCS} ${MARKDOWN_RESOURCES_CPP})

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)

# PUBLIC：公开头用到 Qt 类型，宿主得连着拿到。
# qlitehtml / md4c 只在 .cpp 里出现，链接与头文件路径都不外传
target_link_libraries(${TARGET_NAME}
    PUBLIC
    Qt5::Widgets
    Qt5::Gui
    Qt5::Core
    PRIVATE
    qlitehtml
    md4c
)

target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)
endif()
