# kissfft + QCustomPlot 的频谱演示：合成信号 → 加窗 → 实数 FFT → 画时域与单边
# 幅度谱。FFT 用 kissfft——它是本仓唯一的一等 FFT 依赖（见顶层那段说明）
set(TARGET_NAME "fft_demo")

set(ALL_SRCS
    main.cpp
    fft_demo_window.cpp
    fft_demo_window.h
    fft_processor.cpp
    fft_processor.h
)

add_executable(${TARGET_NAME} WIN32 ${ALL_SRCS})

# kissfft 的头文件路径由它自己的 target 带出来（PUBLIC 的 BUILD_INTERFACE），
# 不必像 FFTW 那样手动补 include
target_link_libraries(${TARGET_NAME} PRIVATE
    qcustomplot
    kissfft
)

# 宏归一口径：fluent 才有宏（APP_UI_FLUENT），qss 是缺省无宏。本示例的 fluent
# 分支是空操作（维持系统默认样式），只注宏、不链 FluentUI3Style；qss 缺省路
# 链底座样式表（src/style）——示例没有自己的项目层，看到的就是纯底座层
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

# 部署：fft_demo 经 QCustomPlot 用到 QtPrintSupport，而 app 侧的 make_deploy 只对
# demo_app.exe 跑 windeployqt（依赖按导入表收集），收不到这份 DLL。这里单独扫一次，
# 顶层再把它挂成 make_deploy 的前置——顺序执行，免得两个 windeployqt 同时往 bin/ 写
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
