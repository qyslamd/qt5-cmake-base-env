set(TARGET_NAME "funq_target")

set(ALL_SRCS
    main.cpp
    funq_target_window.cpp
    funq_target_window.h
)

add_executable(${TARGET_NAME} WIN32 ${ALL_SRCS})

# 只链 Qt Widgets：靶子要「控件少而稳、依赖越薄越好」——它一旦链上本仓的
# 业务模块，测试的前置条件就会跟着那些模块的开关走
target_link_libraries(${TARGET_NAME} PRIVATE
    Qt5::Widgets
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)
endif()

configure_run_script("run_${TARGET_NAME}")
