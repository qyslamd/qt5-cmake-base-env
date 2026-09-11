cmake_minimum_required(VERSION 3.19)

#===============================================================================
#
#   项目定制区 —— 基于本模板新建项目时，只需修改这一段
#
#   其余构建设施（版本管理、部署、打包、符号归档、代码统计、单元测试）
#   会自动跟随，无需改动其它文件。详细说明见 docs/guide/模板使用指南.md
#
#-------------------------------------------------------------------------------

# 项目名。同时作为 CMake 工程名与默认的应用名。
# 未修改时 configure 阶段会输出 WARNING 提醒。
set(PROJECT_NAME_STR "QtCMakeAppDemo")

# 组织名。决定 %LOCALAPPDATA% 下的数据目录层级（配置/日志/崩溃），
# 以及安装包的发行者与安装目录层级 {autopf}\<组织名>\<应用名>。
set(APP_ORG_NAME "Enpht")

# 应用名。默认跟随项目名。与组织名共同决定数据目录、单实例键、窗口标题、
# 单实例互斥体名；改名后旧数据目录不会自动迁移。
set(APP_APP_NAME "${PROJECT_NAME_STR}")

# 项目主页 / 支持地址，写入安装包的发行者链接；留空则不显示链接。
set(APP_URL "")

# 各 CMake 目标名的前缀。core / libui / app 子目录的 TARGET_NAME 均由此
# 拼出，得到 <前缀>_core / <前缀>_libui / <前缀>_app，同时决定 exe 文件名、
# 运行脚本名与打包脚本中的目标名。
set(APP_TARGET_PREFIX "demo")

# 安装包产品标识（Inno Setup 的 AppId），x64 与 x86 各一个。
# 每个项目必须换成新的 GUID —— 沿用模板默认值会让 Windows 把不同项目
# 视为同一程序，安装时直接覆盖卸载。可用 pwsh 执行 [guid]::NewGuid() 生成。
set(APP_INSTALLER_ID_X64 "{{028a3caf-84f9-43a5-818f-af4237aac3d6}}")
set(APP_INSTALLER_ID_X86 "{{c11fe96c-bb38-42e2-87ab-bd9d72fb4644}}")

#===============================================================================
#   项目定制区结束。以下内容通常无需修改。
#===============================================================================

project(${PROJECT_NAME_STR}
    VERSION 1.0.0
    DESCRIPTION "a complete qt cmake basic demo."
    LANGUAGES CXX C)

#-------------------------------------------------------------------------------
# 由定制区变量派生，请勿手工修改
#-------------------------------------------------------------------------------

# 目标架构后缀（x64 / x86）。互斥体名与安装包文件名都要带它
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(APP_ARCH_SUFFIX "x64")
    set(APP_INSTALLER_ID "${APP_INSTALLER_ID_X64}")
else()
    set(APP_ARCH_SUFFIX "x86")
    set(APP_INSTALLER_ID "${APP_INSTALLER_ID_X86}")
endif()

# 单实例互斥体名。运行时由 SingleInstance 创建并持有，安装/卸载脚本据
# 此判断程序是否在运行；两处必须同名，故只在此定义一处。
# 必须带架构后缀：否则同机的 x86 与 x64 版本会共用同一互斥体，互相判定
# 为"已在运行"，安装/卸载也会互相阻塞。
set(APP_MUTEX_NAME "${APP_APP_NAME}Mutex_${APP_ARCH_SUFFIX}")

# 模板防呆：下面两项未改说明尚未按定制区改名/换号，产出的程序与安装包
# 会与其它模板产物互相冲突

# 项目名未改：应用名、数据目录与单实例键都会沿用模板取值
if(PROJECT_NAME_STR STREQUAL "QtCMakeAppDemo")
    message(WARNING
        "PROJECT_NAME_STR 仍是模板默认值！新建项目请先改名，"
        " APP_ORG_NAME / APP_APP_NAME 会随之决定数据目录与单实例键。")
endif()

# 安装包 GUID 未换：Inno Setup 以 AppId 识别产品，沿用模板默认值会让
# 不同项目被系统视为同一程序，安装新项目时直接覆盖卸载旧项目
if(APP_INSTALLER_ID_X64 MATCHES "028a3caf-84f9-43a5-818f-af4237aac3d6" OR
   APP_INSTALLER_ID_X86 MATCHES "c11fe96c-bb38-42e2-87ab-bd9d72fb4644")
    message(WARNING
        "APP_INSTALLER_ID_X64 / APP_INSTALLER_ID_X86 仍是模板默认 GUID！"
        "请为 x64/x86 各换一个新 GUID（pwsh 执行 [guid]::NewGuid() 可生成），"
        "否则安装包会互相覆盖。")
endif()

#-------------------------------------------------------------------------------
# Qt 安装路径
#-------------------------------------------------------------------------------

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

#-------------------------------------------------------------------------------
# 构建脚本模块
#-------------------------------------------------------------------------------

# 自动解压第三方依赖库
if (EXISTS "cmake/extract_dependencies.cmake")
    include(cmake/extract_dependencies.cmake)
endif()

# 运行脚本生成宏（configure_run_script），供 app 与 tests 使用
include(cmake/run_app.cmake)

#-------------------------------------------------------------------------------
# CMake 全局配置
#-------------------------------------------------------------------------------

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# 所有 target 的输出统一到 bin 和 lib 下
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
    add_compile_options("/utf-8")
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

#-------------------------------------------------------------------------------
# clangd / clang-tidy 支持
#-------------------------------------------------------------------------------

# 导出 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 生成 .clangd，把 clangd 指向本次配置的构建目录（该文件不入库）
set(CLANGD_COMPILE_COMMANDS_DIR "${CMAKE_BINARY_DIR}")
file(TO_CMAKE_PATH "${CLANGD_COMPILE_COMMANDS_DIR}" CLANGD_COMPILE_COMMANDS_DIR)
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/.clangd.in"
    "${CMAKE_SOURCE_DIR}/.clangd"
    @ONLY
)

#-------------------------------------------------------------------------------
# Qt
#-------------------------------------------------------------------------------

find_package(Qt5 COMPONENTS
    Core Gui Widgets PrintSupport Test Xml Svg Network Concurrent Sql SerialPort OpenGL
    CONFIG REQUIRED)

# Qt 运行时目录推导仅服务 Windows 消费方（run_app.bat、测试环境变量）。
# qmake 按定义位于 Qt 安装的 bin 目录下，由它反推最直接；不要在各子目录
# 另起一套推导，否则改动假设时容易漏改。
if(Qt5_FOUND AND WIN32)
    get_target_property(_qt_qmake Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${_qt_qmake}" DIRECTORY)
    get_filename_component(QT_LIB_DIR "${QT_BIN_DIR}/../lib" ABSOLUTE)
    get_filename_component(QT_PLUGINS_DIR "${QT_BIN_DIR}/../plugins" ABSOLUTE)
endif()

#-------------------------------------------------------------------------------
# 第三方库
#-------------------------------------------------------------------------------

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

#-------------------------------------------------------------------------------
# 本仓库模块
#-------------------------------------------------------------------------------

# 可选 demo（子目录未入库，克隆后为空，可整体删除）
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/demoes)
    add_subdirectory(demoes)
endif()

# src 下的各模块（core / libui / app）
add_subdirectory(src)

# 单元测试（仅在配置了 BUILD_TESTS 时启用）
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#-------------------------------------------------------------------------------
# 代码统计脚本（cloc）
#-------------------------------------------------------------------------------

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

#-------------------------------------------------------------------------------
# clang-tidy 静态分析脚本
#-------------------------------------------------------------------------------

if(WIN32)
    find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
    if(NOT CLANG_TIDY_EXECUTABLE)
        message(WARNING
            "clang-tidy not found! 静态分析脚本不会生成。"
            "请将其加入 PATH，或用 -DCLANG_TIDY_EXECUTABLE=<路径> 指定。")
    else()
        # 脚本内烘焙了本次配置的构建目录；脚本支持 -BuildDir 覆盖
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/clang_tidy.ps1.in"
            "${CMAKE_SOURCE_DIR}/scripts/clang_tidy.ps1"
            @ONLY
        )
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/clang_tidy.bat.in"
            "${CMAKE_SOURCE_DIR}/scripts/clang_tidy.bat"
            @ONLY
        )
    endif()
endif()
