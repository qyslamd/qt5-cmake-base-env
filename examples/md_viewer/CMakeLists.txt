# 独立的 Markdown 查看器：单篇 + 整个目录两种打开方式，渲染全交给
# src/markdown（MarkdownView），这里只做宿主策略。目标是"发给同事也能用"
# 的产品型示例，不是教学样板
set(TARGET_NAME "md_viewer")

set(ALL_SRCS
    main.cpp
    md_viewer_window.cpp
    md_viewer_window.h
)

add_executable(${TARGET_NAME} WIN32 ${ALL_SRCS})

target_link_libraries(${TARGET_NAME} PRIVATE ${APP_TARGET_PREFIX}_markdown)

# 这个示例**固定走 fluent**：FluentUI3Style + Frameless（QWK 无边框壳），
# 不加载任何 QSS，也不跟 APP_UI_ROUTE 走——它就是这个路线的试验场
if(NOT BUILD_FLUENT_UI)
    message(FATAL_ERROR
        "md_viewer 固定走 fluent，需要 BUILD_FLUENT_UI=ON（见顶层构建开关区）")
endif()
target_link_libraries(${TARGET_NAME} PRIVATE
    FluentUI3Style
    Frameless
)
target_compile_definitions(${TARGET_NAME} PRIVATE APP_UI_FLUENT)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc /utf-8)
endif()

# 生成运行脚本（Windows 为 .bat，Linux 为 .sh），输出到可执行文件所在目录
configure_run_script("run_${TARGET_NAME}")

# 部署：与 fft_demo 同一个理由——app 侧的 make_deploy 只对 demo_app.exe 跑
# windeployqt（按导入表收集），收不到本目标的依赖。顶层再把它挂成 make_deploy
# 的前置，顺序执行免得两个 windeployqt 同时往 bin/ 写
if(WIN32 AND EXISTS ${WINDEPLOYQT_EXECUTABLE})
    add_custom_target(deploy_${TARGET_NAME}
        COMMAND ${WINDEPLOYQT_EXECUTABLE}
            --verbose 1
            --no-angle
            --no-opengl-sw
            --no-system-d3d-compiler
            --compiler-runtime
            "$<TARGET_FILE:${TARGET_NAME}>"
        COMMENT "Running windeployqt for ${TARGET_NAME}..."
    )
    add_dependencies(deploy_${TARGET_NAME} ${TARGET_NAME})
endif()
