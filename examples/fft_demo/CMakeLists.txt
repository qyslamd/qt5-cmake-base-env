# FFTW + QCustomPlot 的频谱演示：合成信号 → 加窗 → 单精度 r2c FFT → 画时域与
# 单边幅度谱。FFT 用的是单精度那套（fftw3f），见顶层 BUILD_FFTW 的说明
#
# 依赖关系上它是 BUILD_FFTW 唯一的使用方，开关关掉就整块跳过
if(NOT BUILD_FFTW)
    message(STATUS "BUILD_FFTW=OFF，跳过 FFTW 示例")
    return()
endif()

set(TARGET_NAME "fft_demo")

set(ALL_SRCS
    main.cpp
    fft_demo_window.cpp
    fft_demo_window.h
    fft_processor.cpp
    fft_processor.h
)

add_executable(${TARGET_NAME} WIN32 ${ALL_SRCS})

# FFTW 的 CMakeLists 只登记了 $<INSTALL_INTERFACE:include>，树内消费者拿不到头
# 文件路径，这里自己加。fftw3.h 是自足的（只 include <stdio.h>），所以给源码树
# 的 api/ 就够了，不需要它的构建目录。
#
# 之所以不把这两行挂在 fftw3f 的 INTERFACE 上：上游有 install(TARGETS ... EXPORT
# ...)，导出目标的接口里不允许出现源码树路径，CMake 会在 generate 阶段报错
# （顶层的 BUILD_FFTW 那段有同样的说明）
target_include_directories(${TARGET_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/3rdparty/fftw-3.3.11/api"
)

target_link_libraries(${TARGET_NAME} PRIVATE
    ${APP_TARGET_PREFIX}_style
    qcustomplot
    fftw3f
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)
endif()

configure_run_script("run_${TARGET_NAME}")
