cmake_minimum_required(VERSION 3.19)
set(PROJECT_NAME_STR "QtCMakeAppDemo")
project(${PROJECT_NAME_STR}
    VERSION 1.0.0
    DESCRIPTION "a complete qt cmake basic demo."
    LANGUAGES CXX C)
	
# local.cmake 为 Windows 路径语义，仅 Windows 读取；
# Linux 使用系统 Qt，必要时通过 -DCMAKE_PREFIX_PATH / -DQt5_DIR 指定
if(WIN32)
    if (EXISTS "${CMAKE_SOURCE_DIR}/local.cmake")
        include("${CMAKE_SOURCE_DIR}/local.cmake")
        message(STATUS "Loaded variables from local.cmake")
    else()
        set(PATH_FOR_QT_INSTALLATION_X64 "X:/Qt/Qt5.12.12/5.12.12/msvc2019_64")
        set(PATH_FOR_QT_INSTALLATION_X86 "X:/Qt/Qt5.12.12/5.12.12/msvc2019")
        configure_file("${CMAKE_SOURCE_DIR}/cmake/local.cmake.in"
            "${CMAKE_SOURCE_DIR}/local.cmake"
            @ONLY)
        message(FATAL_ERROR "Please change local.cmake to set your own Qt paths for both x64 and x86, then rerun CMake.")
    endif()
endif()

#=======================================
# 自动解压第三方依赖库
if (EXISTS "cmake/extract_dependencies.cmake")
    include(cmake/extract_dependencies.cmake)
endif()

#=======================================
# CMake全局变量
set(CMAKE_C_STANDARD 11)    # C 标准，影响全局
set(CMAKE_CXX_STANDARD 17)  # C++ 标准，影响全局
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
# 设置输出目录,所有target的输出目录都统一到bin和lib下
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# MSVC
if(MSVC)
    add_compile_options("/utf-8")
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

#=======================================
# 查找Qt库
find_package(Qt5 COMPONENTS 
    Core Gui Widgets PrintSupport Test Xml Svg network Concurrent Sql SerialPort Network OpenGL
    CONFIG REQUIRED)

# Qt 运行时目录推导仅服务 Windows 消费方（run_app.bat、测试环境变量）
if(Qt5_FOUND AND WIN32)
    get_target_property(_qt_core_location Qt5::Core LOCATION)
    get_filename_component(QT_LIB_DIR "${_qt_core_location}" DIRECTORY)
    get_filename_component(QT_BIN_DIR "${QT_LIB_DIR}/../bin" ABSOLUTE)
    get_filename_component(QT_PLUGINS_DIR "${QT_BIN_DIR}/../plugins" ABSOLUTE)
endif()

# spdlog 日志库
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Do not build spdlog examples" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Do not build spdlog tests" FORCE)
add_subdirectory(3rdparty/spdlog-1.17.0)

# googletest 单元测库
add_subdirectory(3rdparty/googletest-1.17.0)

# QCustomPlot（波形图绘制）
add_subdirectory(3rdparty/QCustomPlot)

# QXlsx Excel读写库
add_subdirectory(3rdparty/QXlsx-1.5.0/QXlsx)


# =======================================
# 包含demoes
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/demoes)
    add_subdirectory(demoes)
endif()

# =======================================
# 包含src目录下的CMakeLists.txt，编译应用程序
add_subdirectory(src)

#=======================================
# 生成 Windows 下的代码统计脚本（cloc）
if(WIN32)
    find_program(CLOC_EXECUTABLE
        NAMES cloc cloc-2.06
        HINTS ${CMAKE_SOURCE_DIR}/3rdparty/cloc-2.06
    )
    if(NOT CLOC_EXECUTABLE)
        message(WARNING "cloc executable not found! Put cloc-2.06.exe in 3rdparty/cloc-2.06/")
    else()
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/code_stats.bat.in"
            "${CMAKE_SOURCE_DIR}/scripts/code_stats.bat"
            @ONLY
        )
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/code_stats.ps1.in"
            "${CMAKE_SOURCE_DIR}/scripts/code_stats.ps1"
            @ONLY
        )
    endif()
endif()

# 单元测试（仅在配置了 BUILD_TESTS 时启用）
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()