set(TARGET_NAME "style_sheet_demo")

set(ALL_SRCS
    main.cpp
    demo_helpers.h
    buttons_page.h
    buttons_page.cpp
    input_page.h
    input_page.cpp
    item_view_page.h
    item_view_page.cpp
    container_page.h
    container_page.cpp
    progress_page.h
    progress_page.cpp
    misc_page.h
    misc_page.cpp
    style_sheet_demo_window.h
    style_sheet_demo_window.cpp
)

add_executable(${TARGET_NAME} WIN32 ${ALL_SRCS})

if(APP_UI_ROUTE STREQUAL "fluent")
    target_compile_definitions(${TARGET_NAME} PRIVATE APP_UI_FLUENT)
else()
    target_link_libraries(${TARGET_NAME} PRIVATE ${APP_TARGET_PREFIX}_style)
endif()

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)
endif()

configure_run_script("run_${TARGET_NAME}")