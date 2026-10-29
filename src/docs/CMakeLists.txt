set(TARGET_NAME ${APP_TARGET_PREFIX}_docs)

add_library(${TARGET_NAME} STATIC
    docs_catalog.cpp
    docs_catalog.h
    docs_viewer_window.cpp
    docs_viewer_window.h)

target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${TARGET_NAME} PUBLIC
    qlitemd::markdown_view
    Qt5::Widgets
    Qt5::Gui
    Qt5::Core
    Qt5::PrintSupport)

set_target_properties(${TARGET_NAME} PROPERTIES AUTOMOC ON)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc /utf-8)
endif()
