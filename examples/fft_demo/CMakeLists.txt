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

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)
endif()

configure_run_script("run_${TARGET_NAME}")
