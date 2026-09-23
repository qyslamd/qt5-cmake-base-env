set(TARGET_NAME "style_demo")

set(ALL_SRCS
    main.cpp
)

add_executable(${TARGET_NAME} WIN32 ${ALL_SRCS})

# 只链样式模块：底座层就是它能拿到的全部样式（项目层属于具体某个程序）
target_link_libraries(${TARGET_NAME} PRIVATE
    ${APP_TARGET_PREFIX}_style
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)
endif()

configure_run_script("run_${TARGET_NAME}")
