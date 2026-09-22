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

# 各 CMake 目标名的前缀。core / app 子目录的 TARGET_NAME 均由此拼出，
# 得到 <前缀>_core / <前缀>_app，同时决定 exe 文件名、运行脚本名与
# 打包脚本中的目标名。
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

# 对解压出来的第三方源码应用补丁（详见 cmake/patch_dependencies.cmake）
include(cmake/patch_dependencies.cmake)

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

# 本仓只用 Qt5，这里把版本号显式钉死：部分第三方库（qmsetup 的 qm_find_qt）
# 只在 QT_VERSION_MAJOR 未定义时才做「版本无关查找」——那个查找走的是包名
# QT，认的是 QT_DIR，与本仓 local.cmake 只提供 Qt5_DIR 的写法对不上，会直接
# 配置失败；钉死之后它们转为 find_package(Qt5 ...)，正好复用已解析的 Qt5_DIR
set(QT_VERSION_MAJOR 5)

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

# QtTesting GUI测试库：UI 录制/回放的引擎。
# 开关 BUILD_QT_TESTING 默认 ON，各构建类型都能用。它是静态库，开了会整套
# 链进 exe，并在主窗口挂出「测试」菜单；不需要时（例如准备发布包）用
# -DBUILD_QT_TESTING=OFF 关掉，省一份编译与体积
option(BUILD_QT_TESTING "启用 QtTesting UI 录制/回放" ON)

if(BUILD_QT_TESTING)
    # 该库自带的 9 个上游用例已由 patches/QtTesting/ 的补丁关掉（它们按
    # ParaView 的运行环境编写，实测全部超时，会拖死整轮 ctest），此处不必
    # 再在宿主侧绕它的 BUILD_TESTING
    add_subdirectory(3rdparty/QtTesting)
else()
    message(STATUS "BUILD_QT_TESTING=OFF，跳过 QtTesting（UI 录制/回放不可用）")
endif()

# funq GUI测试库：同样是 UI 测试工具，但用法与 QtTesting 不同——它不链进
# 被测程序，而是在测试运行时把 Funq.dll 注入进去（Windows 走 CreateRemoteThread，
# Linux 走 LD_PRELOAD），详见 3rdparty/funq-master/server/funq_server/runner_*.py。
# 产物落在 bin/Funq.dll 与 lib/FunqStatic.lib，没有任何目标链接它们。
# 开关 BUILD_FUNQ 默认 ON，不需要时（例如准备发布包）用 -DBUILD_FUNQ=OFF 关掉
option(BUILD_FUNQ "构建 funq GUI 测试库（注入式，产物供测试运行时使用）" ON)

if(BUILD_FUNQ)
    # 该库自带的三个测试目标（player_tester / testLibFunq / testProtocole）已由
    # patches/funq/ 的补丁改为默认不编，且其 Qt 查找锁定到 Qt5
    add_subdirectory(3rdparty/funq-master/server)

    # 注入用的库要放在 funq_server 包目录里、且名字与 runner.py 的
    # _find_library 一致（Windows 是 libFunq.dll，Linux 是 libFunq.so）——它
    # 只管那一个位置与那一个名字，而 MSVC 产出的是 Funq.dll，所以要拷一份改名。
    # 用 ALL 目标而非 POST_BUILD：解压产物被删除重建（补丁重放）时对象文件
    # 未必重编，POST_BUILD 不会再触发，库就悄悄消失了
    #
    # 除了无后缀那份，再按**构建类型**存一份带后缀的（libFunq-debug.dll /
    # libFunq-relwithdebinfo.dll …）。理由：暂存位置在源码树里、只有一个，而
    # 构建目录有多个配置，不分名的话后一次构建会覆盖前一次；拿另一个配置的
    # 被测程序去注入就是错配——Debug 的 Qt 二进制（Qt5Cored）与 Release
    # （Qt5Core）互不兼容，表现为 LoadLibrary 失败或目标进程被拖住，而报错
    # 只有一句看不出原因的 "WaitForSingleObject() returned 258"。
    # 带后缀的那份由构建系统的脚本与测试显式传给 runner（--library），
    # 无后缀那份留给手工调用 python -m funq_server.runner（沿用旧行为）
    if(TARGET Funq)
        if(WIN32)
            set(FUNQ_INJECTOR_STAGE_NAME "libFunq.dll")
        else()
            set(FUNQ_INJECTOR_STAGE_NAME "libFunq.so")
        endif()
        set(FUNQ_INJECTOR_STAGE_DIR
            "${CMAKE_SOURCE_DIR}/3rdparty/funq-master/server/funq_server")

        string(TOLOWER "${CMAKE_BUILD_TYPE}" FUNQ_CONFIG_SUFFIX)
        if(NOT FUNQ_CONFIG_SUFFIX STREQUAL "")
            get_filename_component(FUNQ_INJECTOR_STEM "${FUNQ_INJECTOR_STAGE_NAME}" NAME_WE)
            get_filename_component(FUNQ_INJECTOR_EXT "${FUNQ_INJECTOR_STAGE_NAME}" EXT)
            set(FUNQ_INJECTOR_CONFIGURED_NAME
                "${FUNQ_INJECTOR_STEM}-${FUNQ_CONFIG_SUFFIX}${FUNQ_INJECTOR_EXT}")
        else()
            # 多配置生成器（VS）下 CMAKE_BUILD_TYPE 为空，配置名要到生成期才
            # 知道，故退回无后缀名字——与改动前一致
            set(FUNQ_INJECTOR_CONFIGURED_NAME "${FUNQ_INJECTOR_STAGE_NAME}")
        endif()
        # 供消费方（tests/gallery_funq 的注入器包装、examples/funq 的 serve 脚本）
        # 经 configure_file 注入到生成的脚本里
        set(FUNQ_INJECTOR_LIBRARY
            "${FUNQ_INJECTOR_STAGE_DIR}/${FUNQ_INJECTOR_CONFIGURED_NAME}")

        add_custom_target(stage_funq_injector ALL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Funq>" "${FUNQ_INJECTOR_LIBRARY}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Funq>"
                "${FUNQ_INJECTOR_STAGE_DIR}/${FUNQ_INJECTOR_STAGE_NAME}"
            DEPENDS Funq
            COMMENT "Staging ${FUNQ_INJECTOR_CONFIGURED_NAME}（+ 无后缀那份）next to funq_server")
    endif()
else()
    message(STATUS "BUILD_FUNQ=OFF，跳过 funq（注入式 UI 测试不可用）")
endif()

# SARibbon Ribbon 界面库：把主窗口的菜单栏换成功能区（Ribbon）。静态库，
# 随本仓一次编译。开关 BUILD_RIBBON 默认 ON，不需要时用 -DBUILD_RIBBON=OFF
# 关掉——主窗口会退回普通菜单栏（见 src/app/mainwindow.cpp 的条件编译）
option(BUILD_RIBBON "启用 SARibbon Ribbon 界面（主窗口的功能区菜单）" ON)

if(BUILD_RIBBON)
    # 无边框窗口（frameless）由 QWindowKit 提供，而它要求 Qt ≥ 5.14：低版本
    # 连编都不该编，故这里跳过整个 QWindowKit，SARibbon 侧对应关掉 frameless。
    # 判的是 Qt5_VERSION——find_package(Qt5) 只设置这个变量，QT_VERSION 不由
    # Qt5Config 提供（照着后者写会永远判成"低版本"）
    if(Qt5_VERSION VERSION_LESS "5.14")
        message(STATUS
            "Qt ${Qt5_VERSION} < 5.14：跳过 QWindowKit，SARibbon 的无边框支持关闭"
            "（Ribbon 本体不受影响）")
        set(SARIBBON_USE_FRAMELESS_LIB OFF CACHE BOOL "关闭无边框窗口" FORCE)
    else()
        set(QWINDOWKIT_BUILD_STATIC ON CACHE BOOL "把 QWindowKit 编成静态库" FORCE)
        set(QWINDOWKIT_BUILD_WIDGETS ON CACHE BOOL "编 QWindowKit 的 widgets 模块" FORCE)
        set(QWINDOWKIT_BUILD_QUICK OFF CACHE BOOL "不编 quick 模块" FORCE)
        set(QWINDOWKIT_BUILD_EXAMPLES OFF CACHE BOOL "不编 QWindowKit 示例" FORCE)
        set(QWINDOWKIT_BUILD_DOCUMENTATIONS OFF CACHE BOOL "不生成文档" FORCE)
        set(QWINDOWKIT_INSTALL OFF CACHE BOOL "不安装 QWindowKit" FORCE)
        set(SARIBBON_USE_FRAMELESS_LIB ON CACHE BOOL "启用无边框窗口" FORCE)
        add_subdirectory(3rdparty/qwindowkit-1.5.0)
    endif()

    # 这两个选项必须在 add_subdirectory 之前设好：SARibbon 用 option() 声明，
    # 缓存里已有条目时它不会覆盖
    set(SARIBBON_BUILD_STATIC_LIBS ON CACHE BOOL "把 SARibbonBar 编成静态库" FORCE)
    set(SARIBBON_BUILD_EXAMPLES OFF CACHE BOOL "不编 SARibbon 自带示例" FORCE)
    set(SARIBBON_INSTALL_IN_CURRENT_DIR OFF CACHE BOOL "不安装到当前目录" FORCE)
    add_subdirectory(3rdparty/SARibbon-2.5.7)
else()
    message(STATUS "BUILD_RIBBON=OFF，跳过 SARibbon（主窗口用普通菜单栏）")
endif()

#-------------------------------------------------------------------------------
# 外部源码仓库（git submodule）
#-------------------------------------------------------------------------------

# qt-libui 通用 UI 部件。以 submodule 引入并源码内嵌，随本仓一次构建：
# 库内按「非顶层构建」处理——不读它自己的 local.cmake（Qt 由本仓解析）、
# 不登记 install 规则，只提供 qt-libui::libui 目标。
# 克隆本仓后若该目录为空，先执行 git submodule update --init --recursive
#
# gallery 示例是本仓 funq UI 测试的被测程序（控件多，适合当靶场）。它默认
# 只在 qt-libui 作为顶层项目时编，本仓把 qt-libui 当子模块，故在此显式打开
option(BUILD_LIBUI_GALLERY
    "构建 qt-libui 的 gallery 示例（funq UI 测试的被测程序）" ON)
if(BUILD_LIBUI_GALLERY)
    set(BUILD_EXAMPLES ON)
endif()
add_subdirectory(external/qt-libui)

#-------------------------------------------------------------------------------
# 本仓库模块
#-------------------------------------------------------------------------------

# src 下的各模块（core / app）
add_subdirectory(src)

# funq 的手动分离运行示例（两个终端各起一边）。依赖上面建出来的 demo_app
# 目标，所以放在 src 之后。不需要时删掉这一行即可
add_subdirectory(examples)

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
