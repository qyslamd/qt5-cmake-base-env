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
# Ninja 版本闸门
#
# ninja 是构建链里版本不对还会安静出错的那个：VS2019 各版本安装带的 ninja
# 版本不同（有的仍停在 1.8.2），与 CMake 生成的依赖规则配合会出问题。
# 预设与构建脚本已固定指向 3rdparty/ninja.exe，这里挡的是绕过预设的配置，
# 主要是缓存里存着旧路径的既有构建目录。
#-------------------------------------------------------------------------------

if(CMAKE_GENERATOR STREQUAL "Ninja")
    execute_process(
        COMMAND "${CMAKE_MAKE_PROGRAM}" --version
        OUTPUT_VARIABLE NINJA_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE NINJA_PROBE_RESULT
    )
    if(NINJA_PROBE_RESULT OR NINJA_VERSION VERSION_LESS 1.10.0)
        message(FATAL_ERROR
            "ninja 不可用或版本过低：CMAKE_MAKE_PROGRAM=\"${CMAKE_MAKE_PROGRAM}\"，"
            "实测版本 \"${NINJA_VERSION}\"，要求 >= 1.10.0。\n"
            "改用仓库自带的那份 3rdparty/ninja.exe：\n"
            "  既有构建目录：cmake -B <构建目录> -U CMAKE_MAKE_PROGRAM\n"
            "  新构建目录  ：scripts/build_ninja.bat（预设已固定指向它）")
    endif()
endif()

#===============================================================================
#   构建开关区 —— 本仓全部 option()/CACHE 开关集中声明于此
#
#   覆盖方式：cmake --preset <名>（预设见 CMakePresets.json，可给预设加
#   cacheVariables 固化某组开关），或 -D<开关>=<值>；改完重新 configure 生效
#
#   两个"分发块"受约束必须留在后文原位，勿搬到这里：
#     - add_compile_definitions(...)：目录作用域，须先于 add_subdirectory(src)
#     - include(cmake/crashpad/crashpad.cmake)：须后于 3rdparty 的解压步骤
#===============================================================================

#-------------------------------------------------------------------------------
# APP_CRASH_BACKEND —— 崩溃捕获后端（三态）
#
#   crashpad —— 进程外捕获（crashpad_handler + minidump）。默认，代价是拖进
#               crashpad / mini_chromium / zlib / linux-syscall-support 四个包
#   builtin  —— 自研：VEH / signal + 平台符号解析，写 .log + .dmp，零第三方依赖
#   none     —— 本仓不装任何捕获，崩溃信号留给使用者自己的方案
#
# 用 CACHE STRING 而不是几个布尔：布尔叠加会产生非法组合，而 STRINGS 属性让
# cmake-gui 直接给下拉框，命令行 -DAPP_CRASH_BACKEND=builtin 也好记
#-------------------------------------------------------------------------------

set(APP_CRASH_BACKEND "crashpad" CACHE STRING
    "崩溃捕获后端：crashpad / builtin / none")

# 迁移旧开关：APP_ENABLE_CRASHPAD 曾是唯一的布尔开关，老构建目录的缓存里还有
# 它。按它的值映射一次再清掉——直接 FATAL 会让所有人手上的 build 目录一
# configure 就报错。只映射"曾经是 OFF"这一种（旧 OFF 等于自研）；旧 ON 与新
# 默认值同义，清掉缓存即可
if(DEFINED CACHE{APP_ENABLE_CRASHPAD})
    if(NOT APP_ENABLE_CRASHPAD AND APP_CRASH_BACKEND STREQUAL "crashpad")
        set(APP_CRASH_BACKEND "builtin" CACHE STRING
            "崩溃捕获后端：crashpad / builtin / none" FORCE)
        message(STATUS
            "APP_ENABLE_CRASHPAD=OFF 已映射为 APP_CRASH_BACKEND=builtin")
    endif()
    unset(APP_ENABLE_CRASHPAD CACHE)
endif()

set_property(CACHE APP_CRASH_BACKEND PROPERTY STRINGS crashpad builtin none)
if(NOT APP_CRASH_BACKEND MATCHES "^(crashpad|builtin|none)$")
    message(FATAL_ERROR "APP_CRASH_BACKEND 只能是 crashpad / builtin / none，"
        "当前是 ${APP_CRASH_BACKEND}")
endif()

# 派生给 CMake 侧用（源码选择、链接库、打包脚本都读这两个），C++ 侧的宏在
# 后文"第三方库"段落统一加
if(APP_CRASH_BACKEND STREQUAL "crashpad")
    set(APP_ENABLE_CRASHPAD ON)
    set(APP_ENABLE_CRASH_CAPTURE OFF)
elseif(APP_CRASH_BACKEND STREQUAL "builtin")
    set(APP_ENABLE_CRASHPAD OFF)
    set(APP_ENABLE_CRASH_CAPTURE ON)
else()
    set(APP_ENABLE_CRASHPAD OFF)
    set(APP_ENABLE_CRASH_CAPTURE OFF)
endif()
message(STATUS "崩溃捕获后端：${APP_CRASH_BACKEND}")

#-------------------------------------------------------------------------------
# APP_ENABLE_AUTH —— 登录认证与用户管理
#
# OFF 时：core 不编 auth/，app 不编登录 / 改密 / 用户管理三个对话框，main 跳过
# 登录直接进主窗口。注意这不是"关掉一个功能"那么简单——它是一道贯穿 core 与
# app 的切割面，改这个开关请连带看 main.cpp 的登录段与 Splash 的会话步
#
# 坑：从 OFF 切回 ON 时，AUTOMOC 的产物不会因为源文件重新入列而重生成——
# <build>/src/core/demo_core_autogen/mocs_compilation.cpp 里还缺 moc_auth_service，
# 链接会报 AuthService::staticMetaObject / loginSucceeded 未解析（实测）。切回
# ON 后先删掉那个 demo_core_autogen 目录再构建（或换个干净的构建目录）
#-------------------------------------------------------------------------------

option(APP_ENABLE_AUTH "启用登录认证与用户管理" ON)

#-------------------------------------------------------------------------------
# APP_ENABLE_CRASH_TEST —— 崩溃测试选项
#
# ON 时给 src/app 定义 APP_ENABLE_CRASH_TEST，于是多出一个 --crash 选项：主动
# 触发一次崩溃，用来跑通"崩溃捕获 → 导出报告 → 开发机符号化"整条链路（方案
# 与验收口径见 docs/plan/plan总结.md 的「崩溃报告回传链路」）。发布构建若不想
# 带这个开关就设为 OFF——
# 客户不会去敲它，但一个"能让程序主动崩掉"的参数不该出现在发布包里
#-------------------------------------------------------------------------------

option(APP_ENABLE_CRASH_TEST "Enable --crash to trigger a test crash" ON)

#-------------------------------------------------------------------------------
# BUILD_QT_TESTING —— QtTesting UI 录制/回放
#
# 默认 ON，各构建类型都能用。它是静态库，开了会整套链进 exe，并在主窗口挂出
# 「测试」菜单；不需要时（例如准备发布包）用 -DBUILD_QT_TESTING=OFF 关掉，
# 省一份编译与体积
#-------------------------------------------------------------------------------

option(BUILD_QT_TESTING "启用 QtTesting UI 录制/回放" ON)

#-------------------------------------------------------------------------------
# BUILD_FUNQ —— funq 注入式 GUI 测试库
#
# 与 QtTesting 同为 UI 测试工具但用法不同：不链进被测程序，而是测试运行时把
# Funq.dll 注入进去（机制见后文"第三方库"段落）。默认 ON，不需要时（例如准备
# 发布包）用 -DBUILD_FUNQ=OFF 关掉
#-------------------------------------------------------------------------------

option(BUILD_FUNQ "构建 funq GUI 测试库（注入式，产物供测试运行时使用）" ON)

#-------------------------------------------------------------------------------
# BUILD_RIBBON —— SARibbon 功能区界面
#
# 默认 ON。OFF 时主窗口的菜单栏那一行退回普通菜单栏（见 src/app/mainwindow.cpp
# 的条件编译），同时不再编 SARibbon / QWindowKit 两个库
#-------------------------------------------------------------------------------

option(BUILD_RIBBON "启用 SARibbon Ribbon 界面（主窗口的功能区菜单）" ON)

#-------------------------------------------------------------------------------
# BUILD_FLUENT_UI —— 第二条界面路线（FluentUI3Style）
#
# 默认 ON。编 FluentUI3Style / Frameless / ExWidgets 三个库（外加 kissfft），
# 产物落 bin/，与 QSS + SARibbon 那条路线并存、由使用者挑一条走（见
# APP_UI_ROUTE）。不编 Qt 样式插件——它会往 Qt 安装目录写文件，与"自包含"相悖。
# OFF 时这条路线整体不进构建
#-------------------------------------------------------------------------------

option(BUILD_FLUENT_UI "构建 FluentUI3Style 界面库（第二条界面路线）" ON)

#-------------------------------------------------------------------------------
# APP_UI_ROUTE —— 主界面走哪条路
#
# qss    ：QSS 项目层样式表（src/app/resources/styles/default.qss）+ SARibbon
# fluent ：FluentUI3Style（QProxyStyle，不加载任何 QSS）+ Frameless 标题栏
#
# 两者在同一进程里互斥：只要加载了样式表，命中规则的控件就走 QStyleSheetStyle，
# FluentUI3Style 的绘制被绕过；窗口框架同理（一个顶层窗口只能装一个 QWindowKit
# window agent）。故只能二选一，不做运行期混用
#-------------------------------------------------------------------------------

set(APP_UI_ROUTE "qss" CACHE STRING "主界面路线：qss / fluent")
set_property(CACHE APP_UI_ROUTE PROPERTY STRINGS qss fluent)
if(NOT APP_UI_ROUTE STREQUAL "qss" AND NOT APP_UI_ROUTE STREQUAL "fluent")
    message(FATAL_ERROR
        "APP_UI_ROUTE 只能是 qss 或 fluent（当前：${APP_UI_ROUTE}）")
endif()

#-------------------------------------------------------------------------------
# BUILD_TESTS —— 单元测试
#
# 默认 ON。OFF 时不注册任何 ctest 用例（tests/ 整个不进构建）
#-------------------------------------------------------------------------------

option(BUILD_TESTS "Build unit tests" ON)

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
        configure_file("${CMAKE_SOURCE_DIR}/cmake/template/local.cmake.in"
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
    "${CMAKE_SOURCE_DIR}/cmake/template/.clangd.in"
    "${CMAKE_SOURCE_DIR}/.clangd"
    @ONLY
)

#-------------------------------------------------------------------------------
# Qt
#-------------------------------------------------------------------------------

find_package(Qt5 COMPONENTS
    Core Gui Widgets PrintSupport Test Xml Svg Network Concurrent Sql SerialPort OpenGL
    CONFIG REQUIRED)

# 翻译工具（lrelease）单独找一次，刻意不与上一行合并、也不带 REQUIRED：
# 它来自 Qt 的 qttools（Debian 系是 qttools5-dev-tools），装机时可能没有，
# 缺了只该失去翻译，不该让整个配置失败。
#
# 为什么不能直接 find_package(Qt5LinguistTools)：本仓 local.cmake 只给
# Qt5_DIR、不设 CMAKE_PREFIX_PATH，而 CMake 不会因为 Qt5_DIR 指向 Qt 的
# lib/cmake 就去那里找别的包——实测直接找是 NOTFOUND。走 Qt5 的 components
# 机制则由 Qt5Config 自己按 Qt5_DIR 解析出 Qt5LinguistTools_DIR
find_package(Qt5 COMPONENTS LinguistTools CONFIG QUIET)
if(NOT Qt5LinguistTools_FOUND)
    message(STATUS
        "未找到 Qt5LinguistTools（lrelease）：跳过翻译编译，"
        "第三方控件的自带文案保持英文")
endif()

# 本仓只用 Qt5，这里把版本号显式钉死：部分第三方库（qmsetup 的 qm_find_qt）
# 只在 QT_VERSION_MAJOR 未定义时才做「版本无关查找」——那个查找走的是包名
# QT，认的是 QT_DIR，与本仓 local.cmake 只提供 Qt5_DIR 的写法对不上，会直接
# 配置失败；钉死之后它们转为 find_package(Qt5 ...)，正好复用已解析的 Qt5_DIR
set(QT_VERSION_MAJOR 5)

# Qt 的目录推导集中在这里，不要在各子目录另起一套，否则改动假设时容易漏改。
if(Qt5_FOUND)
    get_target_property(_qt_qmake Qt5::qmake IMPORTED_LOCATION)

    # Qt 自带翻译（qt_zh_CN.qm 等）所在目录。这里必须问 qmake：Windows 侧它
    # 是 <Qt>/translations，能由 bin 反推，但 Debian 系在 /usr/share/qt5/
    # translations，离 bin 很远，推不出来。构建时要把 qt_zh_CN.qm 拷到可执行
    # 文件旁（见 src/app/CMakeLists.txt）
    execute_process(COMMAND "${_qt_qmake}" -query QT_INSTALL_TRANSLATIONS
        OUTPUT_VARIABLE QT_TRANSLATIONS_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    # 下面这几个仅服务 Windows 消费方（run_app.bat、测试环境变量）
    if(WIN32)
        get_filename_component(QT_BIN_DIR "${_qt_qmake}" DIRECTORY)
        get_filename_component(QT_LIB_DIR "${QT_BIN_DIR}/../lib" ABSOLUTE)
        get_filename_component(QT_PLUGINS_DIR "${QT_BIN_DIR}/../plugins" ABSOLUTE)
    endif()
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

# 崩溃捕获后端：宏与第三方引入（三态见顶部 APP_CRASH_BACKEND 的说明）。
# 必须在 add_subdirectory(src) 之前：
#   - src/core 和 src/app 都要能看见宏
#   - src/app 要能链接 crashpad_client / crashpad_util 等目标
if(APP_ENABLE_CRASHPAD)
    add_compile_definitions(APP_ENABLE_CRASHPAD)
    include(${CMAKE_SOURCE_DIR}/cmake/crashpad/crashpad.cmake)
elseif(APP_ENABLE_CRASH_CAPTURE)
    # 自研方案零第三方依赖，只需要这个宏让 src/core 编那套实现
    add_compile_definitions(APP_ENABLE_CRASH_CAPTURE)
endif()
# none：两个宏都不定义，src/core 两个实现都不编，崩溃信号留给使用者

# 登录认证：宏给 src/core 与 src/app 做条件编译
if(APP_ENABLE_AUTH)
    add_compile_definitions(APP_ENABLE_AUTH)
endif()

# QtTesting GUI测试库：UI 录制/回放的引擎。（开关声明见顶部「构建开关区」）
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
# （开关声明见顶部「构建开关区」）
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
        # 供消费方（tests/funq_demo 的注入器包装、examples/funq-scripts 的
        # serve 脚本）
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
            COMMENT "Staging ${FUNQ_INJECTOR_CONFIGURED_NAME} next to funq_server")
    endif()
else()
    message(STATUS "BUILD_FUNQ=OFF，跳过 funq（注入式 UI 测试不可用）")
endif()

#-------------------------------------------------------------------------------
# QWindowKit —— 无边框窗口（两条界面路线共用）
#
# 这段**必须排在 SARibbon 之前**：SARibbon 在配置期就检查
# `TARGET QWindowKit::Widgets` 决定要不要编它的 frameless 支持
# （见 3rdparty/SARibbon-2.5.7/CMakeLists.txt 的 _SARIBBON_USE_FRAMELESS_LIB），
# 排反了它会把无边框整个关掉——窗口失去自绘的圆角与标题栏，而配置输出里只有
# 一行 WARNING（实测踩过：宏变成 SARIBBON_USE_3RDPARTY_FRAMELESSHELPER=0）
#
# 从 BUILD_RIBBON 段里提出来了：它不再只是 SARibbon 的依赖，FluentUI3Style 的
# Frameless 也链它。两条路线共用本仓编的这一份；FluentUIStyle 靠
# QWindowKit::Widgets 别名判断宿主已提供，就不会去编它自带的那份——否则同一个
# 仓里会出现两套 QWindowKit 栈
#-------------------------------------------------------------------------------
if(BUILD_RIBBON OR BUILD_FLUENT_UI)
    if(Qt5_VERSION VERSION_LESS "5.14")
        message(STATUS
            "Qt ${Qt5_VERSION} < 5.14：跳过 QWindowKit，两条路线的无边框窗口均不可用")
    else()
        set(QWINDOWKIT_BUILD_STATIC ON CACHE BOOL "把 QWindowKit 编成静态库" FORCE)
        set(QWINDOWKIT_BUILD_WIDGETS ON CACHE BOOL "编 QWindowKit 的 widgets 模块" FORCE)
        set(QWINDOWKIT_BUILD_QUICK OFF CACHE BOOL "不编 quick 模块" FORCE)
        set(QWINDOWKIT_BUILD_EXAMPLES OFF CACHE BOOL "不编 QWindowKit 示例" FORCE)
        set(QWINDOWKIT_BUILD_DOCUMENTATIONS OFF CACHE BOOL "不生成文档" FORCE)
        set(QWINDOWKIT_INSTALL OFF CACHE BOOL "不安装 QWindowKit" FORCE)
        add_subdirectory(3rdparty/qwindowkit-1.5.0)

        # 别名由 qwindowkit 无条件创建（src/CMakeLists.txt:88），这里只是断言：
        # 一旦它消失，FluentUIStyle 会静默改编自带那份，两套 QWK 栈同进程
        if(NOT TARGET QWindowKit::Widgets)
            message(FATAL_ERROR
                "QWindowKit::Widgets 别名不存在：FluentUIStyle 会改编它自带的 "
                "QWindowKit；请检查 3rdparty/qwindowkit-1.5.0 是否被改动")
        endif()
    endif()
endif()

#-------------------------------------------------------------------------------
# SARibbon Ribbon 界面库 —— 把主窗口的菜单栏换成功能区
#
# （开关声明见顶部「构建开关区」；必须排在 QWindowKit 之后，理由见上一段）
#-------------------------------------------------------------------------------
if(BUILD_RIBBON)
    # 无边框支持只看目标在不在——比自己再判一遍 Qt 版本可靠：上游
    # SARibbon-2.5.7/CMakeLists.txt 就是 `if(TARGET QWindowKit::Widgets)` 决定
    # 要不要编那段，这里与它保持同一个判据
    if(TARGET QWindowKit::Widgets)
        set(SARIBBON_USE_FRAMELESS_LIB ON CACHE BOOL "启用无边框窗口" FORCE)
    else()
        message(STATUS
            "没有 QWindowKit 目标：SARibbon 的无边框支持关闭（Ribbon 本体不受影响）")
        set(SARIBBON_USE_FRAMELESS_LIB OFF CACHE BOOL "关闭无边框窗口" FORCE)
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
# kissfft —— 基础环境的 FFT 库
#
# 选它不选 FFTW：kissfft 是 BSD-3-Clause / Unlicense 双许可，FFTW 是 GPL-2.0
# ——本仓是给人做业务的基础环境，GPL 会把业务代码一起拽进去；附带好处是它只有
# 5 个 .c，不像 FFTW 每次 configure 都要跑编译器与 SIMD 探测
#
# 四个 cache 变量必须在 add_subdirectory 之前设好：
#   TEST / TOOLS 默认 ON，而它们各自的 CMakeLists 里是
#     pkg_check_modules(... REQUIRED ...)（fftw3 / libpng）——缺 pkg-config
#     时 configure 直接 FATAL
#   STATIC 默认 OFF，不改会编出要部署、要打包的 kissfft-float.dll
#   PKGCONFIG 只是少一条 install(FILES *.pc)
# KISSFFT_DATATYPE 默认 float，与 fft_demo、ExWidgets 的用法一致，不动
#-------------------------------------------------------------------------------
set(KISSFFT_STATIC ON CACHE BOOL "把 kissfft 编成静态库" FORCE)
set(KISSFFT_TEST OFF CACHE BOOL "不编 kissfft 测试（它要 pkg-config 找 fftw3）" FORCE)
set(KISSFFT_TOOLS OFF CACHE BOOL "不编 kissfft 工具（它要 pkg-config 找 libpng）" FORCE)
set(KISSFFT_PKGCONFIG OFF CACHE BOOL "不生成 pkg-config 文件" FORCE)
add_subdirectory(3rdparty/kissfft-131.2.0)

#-------------------------------------------------------------------------------
# FluentUI3Style —— 第二条界面路线（WinUI3/Fluent 风格的 QProxyStyle）
#
# 必须排在 QWindowKit 与 kissfft **之后**：它的 Frameless 靠
# `if(NOT TARGET QWindowKit::Widgets)` 决定用宿主那份还是自带的，ExWidgets 靠
# `if(NOT TARGET kissfft)` 同理——先定义好，它自带的两个副本都不参与构建
#
# BUILD_PLUGIN=OFF：那是 Qt 样式插件，默认还会往 Qt 安装目录写文件，与自包含
# 相悖；EXAMPLES / QTCREATOR_PLUGIN 同理。ExWidgets 上游没给开关（无条件编），
# 随库一起编但不进安装包（设计文档 D2）
#-------------------------------------------------------------------------------
if(BUILD_FLUENT_UI)
    set(BUILD_PLUGIN OFF CACHE BOOL "不编 Qt 样式插件" FORCE)
    set(BUILD_LIBRARY ON CACHE BOOL "编 FluentUI3Style 本体" FORCE)
    set(BUILD_FRAMELESS ON CACHE BOOL "编 Frameless（QWindowKit 无边框）" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "不编 FluentUIStyle 示例" FORCE)
    set(BUILD_QTCREATOR_PLUGIN OFF CACHE BOOL "不编 Qt Creator 插件" FORCE)
    # 不生成它那套 install 规则，两个理由：
    #   1) 它会 install(EXPORT FramelessTargets)，而 Frameless 链的 QWKWidgets
    #      没进任何导出集（本仓 QWINDOWKIT_INSTALL=OFF），生成阶段直接报
    #      "requires target QWKWidgets that is not in any export set"
    #   2) Linux 的 AppImage 走 cmake --install，规则留着会把它的头文件与
    #      cmake 包配置装进 AppDir
    # 变量只在 add_subdirectory 调用时被继承：给这一支设完就撤，本仓自己的
    # 安装规则（Linux 的 AppImage 走 cmake --install）不受影响
    set(CMAKE_SKIP_INSTALL_RULES TRUE)
    add_subdirectory(3rdparty/FluentUIStyle-4.0)
    unset(CMAKE_SKIP_INSTALL_RULES)
else()
    message(STATUS "BUILD_FLUENT_UI=OFF，跳过 FluentUI3Style（界面只有 qss 一条路线）")
endif()

#-------------------------------------------------------------------------------
# 本仓库模块
#-------------------------------------------------------------------------------

# src 下的各模块（core / app）
add_subdirectory(src)

# funq 的手动分离运行示例（两个终端各起一边）。依赖上面建出来的 demo_app
# 目标，所以放在 src 之后。不需要时删掉这一行即可
add_subdirectory(examples)

# 单元测试（开关声明见顶部「构建开关区」）
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
            "${CMAKE_SOURCE_DIR}/cmake/template/code_stats.bat.in"
            "${CMAKE_SOURCE_DIR}/scripts/code_stats.bat"
            @ONLY
        )
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/template/code_stats.ps1.in"
            "${CMAKE_SOURCE_DIR}/scripts/code_stats.ps1"
            @ONLY
        )
    endif()
endif()

#-------------------------------------------------------------------------------
# clang 静态分析与格式化脚本
#
# 自带的那份（3rdparty/clang）一路优先，而不是交给 find_program 的 HINTS：
# find_program 只在缓存里没有结果时才重新搜索，改了 HINTS 也不会重搜，既有
# 构建目录会一直用着第一次找到的那份（同 CLAUDE.md 规则 19）。版本固定正是
# 这里的目的——clang-format 的排版结果随版本变，各人用各自装的那份，diff 里
# 就会混进纯格式噪声。自带那份不在时才去搜 PATH，那时显式 -D 指定才起作用。
#-------------------------------------------------------------------------------

if(WIN32)
    # 缓存里的旧搜索结果（例如第一次配置时找到的别的安装）一并清掉：下面直接
    # 把自带那份写进变量，缓存留着只会让 grep 缓存的人以为它在被使用
    unset(CLANG_TIDY_EXECUTABLE CACHE)
    set(CLANG_TIDY_EXECUTABLE "${CMAKE_SOURCE_DIR}/3rdparty/clang/clang-tidy.exe")
    if(NOT EXISTS "${CLANG_TIDY_EXECUTABLE}")
        unset(CLANG_TIDY_EXECUTABLE)
        find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
    endif()
    if(NOT CLANG_TIDY_EXECUTABLE)
        message(WARNING
            "clang-tidy not found! 静态分析脚本不会生成。"
            "把 clang-tidy.exe 放进 3rdparty/clang/、加入 PATH，"
            "或用 -DCLANG_TIDY_EXECUTABLE=<路径> 指定。")
    else()
        # 脚本内烘焙了本次配置的构建目录；脚本支持 -BuildDir 覆盖
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/template/clang_tidy.ps1.in"
            "${CMAKE_SOURCE_DIR}/scripts/clang_tidy.ps1"
            @ONLY
        )
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/template/clang_tidy.bat.in"
            "${CMAKE_SOURCE_DIR}/scripts/clang_tidy.bat"
            @ONLY
        )
    endif()

    # 格式化脚本不需要构建目录：默认只处理 git 里改动过的文件
    unset(CLANG_FORMAT_EXECUTABLE CACHE)
    set(CLANG_FORMAT_EXECUTABLE "${CMAKE_SOURCE_DIR}/3rdparty/clang/clang-format.exe")
    if(NOT EXISTS "${CLANG_FORMAT_EXECUTABLE}")
        unset(CLANG_FORMAT_EXECUTABLE)
        find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)
    endif()
    if(NOT CLANG_FORMAT_EXECUTABLE)
        message(WARNING
            "clang-format not found! 格式化脚本不会生成。"
            "把 clang-format.exe 放进 3rdparty/clang/、加入 PATH，"
            "或用 -DCLANG_FORMAT_EXECUTABLE=<路径> 指定。")
    else()
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/template/clang_format.ps1.in"
            "${CMAKE_SOURCE_DIR}/scripts/clang_format.ps1"
            @ONLY
        )
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/template/clang_format.bat.in"
            "${CMAKE_SOURCE_DIR}/scripts/clang_format.bat"
            @ONLY
        )
    endif()
endif()

#-------------------------------------------------------------------------------
# 代码统计脚本（cloc）
#
# 模板一直在库里，只是没人接线——生成的脚本带本机的 cloc 路径与源码根，所以
# 和 clang_tidy 一样由 configure_file 产出，不入「用户要改的脚本」那一类
#-------------------------------------------------------------------------------

if(WIN32)
    find_program(CLOC_EXECUTABLE
        NAMES cloc cloc-2.06
        HINTS ${CMAKE_SOURCE_DIR}/3rdparty/cloc-2.06
    )
    if(NOT CLOC_EXECUTABLE)
        message(WARNING
            "cloc executable not found! 代码统计脚本不会生成。"
            "把 cloc-2.06.exe 放进 3rdparty/cloc-2.06/，或用 -DCLOC_EXECUTABLE=<路径> 指定。")
    else()
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/template/code_stats.bat.in"
            "${CMAKE_SOURCE_DIR}/scripts/code_stats.bat"
            @ONLY
        )
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/template/code_stats.ps1.in"
            "${CMAKE_SOURCE_DIR}/scripts/code_stats.ps1"
            @ONLY
        )
    endif()
endif()
