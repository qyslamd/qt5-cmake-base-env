cmake_minimum_required(VERSION 3.19)

message(STATUS "CMake version: ${CMAKE_VERSION}")

#===============================================================================
#   项目定制区 —— 新建项目只需改这一段，其余构建设施自动跟随
#   （版本管理、部署、打包、符号归档、代码统计、单元测试，
#   说明见 docs/guide/模板使用指南.md）
#-------------------------------------------------------------------------------

# 项目名：同时是 CMake 工程名与默认应用名，未改时 configure 会 WARNING
set(PROJECT_NAME_STR "QtCMakeAppDemo")

# 组织名：决定 %LOCALAPPDATA% 数据目录层级，以及安装包的发行者与安装目录
set(APP_ORG_NAME "ZYH")

# 应用名：与组织名共同决定数据目录、单实例键与窗口标题；改名不迁移旧数据
set(APP_APP_NAME "${PROJECT_NAME_STR}")

# 项目主页 / 支持地址，写入安装包的发行者链接；留空则不显示
set(APP_URL "")

# 目标名前缀：拼出 <前缀>_core / <前缀>_app，也决定 exe 与各脚本里的名字
set(APP_TARGET_PREFIX "demo")

# 项目描述：进 project() 的 DESCRIPTION
set(CMAKE_PROJECT_DESC "A Qt and cmake basic demo project.")

# 安装包产品标识（Inno Setup 的 AppId），x64 / x86 各一个。每个项目必须换新
# GUID：沿用模板值会让 Windows 把不同项目当成同一程序，安装时直接覆盖卸载
# （pwsh 执行 [guid]::NewGuid() 生成）
set(APP_INSTALLER_ID_X64 "{{028a3caf-84f9-43a5-818f-af4237aac3d6}}")
set(APP_INSTALLER_ID_X86 "{{c11fe96c-bb38-42e2-87ab-bd9d72fb4644}}")


#===============================================================================
#   项目定制区结束。以下内容通常无需修改。
#===============================================================================

project(${PROJECT_NAME_STR}
    VERSION 1.0.0
    DESCRIPTION "${CMAKE_PROJECT_DESC}"
    LANGUAGES CXX C)

#-------------------------------------------------------------------------------
# Ninja 版本闸门：VS2019 各版本自带的 ninja 版本不一（有的仍是 1.8.2），与
# CMake 生成的依赖规则配合会安静出错。预设与构建脚本已固定用 3rdparty/ninja.exe，
# 这里挡的是绕过预设的配置——主要是缓存里存着旧路径的既有构建目录
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
#   构建开关区 —— 全部 option()/CACHE 开关集中于此，改完重新 configure 生效
#
#   crashpad 的 include 必须留在后文原位：它要用解压出来的源码与目标，
#   须后于解压步骤、先于 add_subdirectory(src)
#===============================================================================

# 崩溃捕获后端是运行时配置（crash/backend，重启生效），不再有编译期开关
# ——见 src/core/crashhandler/crash.h 的 Backend

option(BUILD_QT_TESTING "启用 QtTesting UI 录制/回放" ON)
option(BUILD_FUNQ "构建 funq GUI 测试库（注入式，产物供测试运行时使用）" ON)
option(BUILD_RIBBON "启用 SARibbon Ribbon 界面（主窗口的功能区菜单）" ON)
option(BUILD_FLUENT_UI "构建 FluentUI3Style 界面库（第二条界面路线）" ON)
# 内置 Markdown 文档预览没有开关，是基座的固定能力。它引入的 qlitehtml 是本仓
# 首个 GPL 类依赖（GPL-3.0-only WITH Qt-GPL-exception-1.0 / 商业双授权），
# 详见 docs/plan/内置Markdown文档预览设计.md 与 docs/guide/第三方依赖补丁.md

# 主界面二选一，同进程互斥：qss（样式表 + SARibbon）/ fluent（FluentUI3Style）。
# 普通变量（非缓存）：子目录可覆盖（如 src/app 自己走 fluent），local.cmake
# 可覆盖（不入库的本地切换）。缓存里若还留着下沉前的 -DAPP_UI_ROUTE=fluent，
# 会被这里的同名普通变量静默遮蔽，所以下面要提醒清掉
if(DEFINED CACHE{APP_UI_ROUTE})
    message(WARNING
        "APP_UI_ROUTE 已下沉为普通变量（子目录/local.cmake 里 set 覆盖），"
        "缓存值 '${APP_UI_ROUTE}' 不再生效。执行 cmake -U APP_UI_ROUTE 清掉本条提示")
endif()
set(APP_UI_ROUTE "qss")
if(NOT APP_UI_ROUTE STREQUAL "qss" AND NOT APP_UI_ROUTE STREQUAL "fluent")
    message(FATAL_ERROR
        "APP_UI_ROUTE 只能是 qss 或 fluent（当前：${APP_UI_ROUTE}）")
endif()

# 单元测试：OFF 时 tests/ 不进构建，也不注册 ctest 用例
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

# 单实例互斥体名：运行时由 SingleInstance 持有，安装/卸载脚本据此判断程序是否
# 在运行；必须带架构后缀，否则同机 x86/x64 两版会互判"已在运行"
set(APP_MUTEX_NAME "${APP_APP_NAME}Mutex_${APP_ARCH_SUFFIX}")

# 模板防呆：下面两项没改说明尚未改名/换号，产物会与其它模板产物冲突
if(PROJECT_NAME_STR STREQUAL "QtCMakeAppDemo")
    message(WARNING
        "PROJECT_NAME_STR 仍是模板默认值！新建项目请先改名，"
        " APP_ORG_NAME / APP_APP_NAME 会随之决定数据目录与单实例键。")
endif()

# Inno 以 AppId 识别产品，沿用模板 GUID 会让新项目覆盖卸载旧项目
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

# local.cmake 仅 Windows 读取（路径语义是 Windows 的）；Linux 用系统 Qt，
# 必要时 -DCMAKE_PREFIX_PATH / -DQt5_DIR 指定
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

# 给解压出来的第三方源码打补丁（注册表见 cmake/patch_dependencies.cmake）
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
    # /utf-8 只给 C/C++：MASM 不认识它，crashpad 的 .asm 每个文件会报一条
    # A4018 invalid command-line option；汇编没有源码编码问题，不需要它
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:/utf-8>")
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

#-------------------------------------------------------------------------------
# clangd 支持
#-------------------------------------------------------------------------------

# 导出 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 生成 .clangd 指向本次构建目录（该文件不入库）
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

# lrelease 单独找一次且不带 REQUIRED（它来自 qttools，装机可能没有，缺了只该
# 失去翻译）；必须走 Qt5 的 components 机制——只给 Qt5_DIR 时直接
# find_package(Qt5LinguistTools) 是 NOTFOUND（见 docs/guide/第三方依赖补丁.md）
find_package(Qt5 COMPONENTS LinguistTools CONFIG QUIET)
if(NOT Qt5LinguistTools_FOUND)
    message(STATUS
        "未找到 Qt5LinguistTools（lrelease）：跳过翻译编译，"
        "第三方控件的自带文案保持英文")
endif()

# 钉死 Qt5：qmsetup（qwindowkit/FluentUIStyle 都带）的 qm_find_qt 只在
# QT_VERSION_MAJOR 未定义时才做「版本无关查找」，那个查找认 QT_DIR、与本仓
# 只给 Qt5_DIR 的写法对不上，会直接配置失败（见 docs/guide/第三方依赖补丁.md）
set(QT_VERSION_MAJOR 5)

# Qt 的目录推导集中在这里，不要在各子目录另起一套，否则改动假设时容易漏改。
if(Qt5_FOUND)
    get_target_property(_qt_qmake Qt5::qmake IMPORTED_LOCATION)

    # Qt 自带翻译目录：必须问 qmake（Debian 系在 /usr/share/qt5/translations，
    # 推不出来）；构建时要把 qt_zh_CN.qm 拷到可执行文件旁
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

# Crashpad（崩溃捕获后端之一）：必须在 add_subdirectory(src) 之前引入——
# src/core 要编它的客户端实现，src/app 要能链接 crashpad_client 等目标。
# 后端是运行时配置（crash/backend），这里不再有编译期分发
include(${CMAKE_SOURCE_DIR}/cmake/crashpad/crashpad.cmake)

# QtTesting —— UI 录制/回放的引擎
if(BUILD_QT_TESTING)
    # 它自带的 9 个上游用例已由 patches/QtTesting/ 关掉（按 ParaView 环境写的，
    # 实测全超时、会拖死整轮 ctest），宿主侧不必再绕它的 BUILD_TESTING
    add_subdirectory(3rdparty/QtTesting)
else()
    message(STATUS "BUILD_QT_TESTING=OFF，跳过 QtTesting（UI 录制/回放不可用）")
endif()

# funq —— 注入式 UI 测试库：不链进被测程序，测试运行时把 Funq.dll 注入进去
# （Windows 走 CreateRemoteThread，Linux 走 LD_PRELOAD）
if(BUILD_FUNQ)
    # 它自带的三个测试目标已由 patches/funq/ 改为默认不编，Qt 查找也锁定 Qt5
    add_subdirectory(3rdparty/funq-master/server)

    # 注入库要放进 funq_server 包目录、且名字与 runner.py 的 _find_library 一致
    # （libFunq.dll / libFunq.so），故拷一份改名；再按**构建类型 + 架构**另存
    # 一份带后缀的（暂存目录在源码树里、所有构建目录共用，少了哪一维都会互相
    # 覆盖：Debug 与 Release 的 Qt 二进制不兼容，错配报一句
    # "WaitForSingleObject() returned 258"；x86 与 x64 撞名则是注入直接失败，
    # 只报 LoadLibraryW() 返回 0）。详见 docs/guide/funq注入式UI测试.md。
    # 用 ALL 目标而非 POST_BUILD：解压产物被补丁重放删除重建时对象文件未必重编，
    # POST_BUILD 不会再触发
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
                "${FUNQ_INJECTOR_STEM}-${APP_ARCH_SUFFIX}-${FUNQ_CONFIG_SUFFIX}${FUNQ_INJECTOR_EXT}")
        else()
            # 多配置生成器（VS）下 CMAKE_BUILD_TYPE 为空，配置名要到生成期才
            # 知道，故退回无后缀名字
            set(FUNQ_INJECTOR_CONFIGURED_NAME "${FUNQ_INJECTOR_STAGE_NAME}")
        endif()
        # 供消费方（tests 的注入器包装、examples 的 serve 脚本）经
        # configure_file 注入到生成的脚本里
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
# QWindowKit —— 无边框窗口，两条界面路线共用
#
# **必须排在 SARibbon 之前**：SARibbon 配置期就查 `TARGET QWindowKit::Widgets`
# 决定要不要编 frameless 支持，排反了会把无边框静默关掉（宏变成
# SARIBBON_USE_3RDPARTY_FRAMELESSHELPER=0，只有一行 WARNING）。FluentUIStyle
# 的 Frameless 也靠这个别名判断宿主已提供，否则会编它自带的那份
#-------------------------------------------------------------------------------
if(BUILD_RIBBON OR BUILD_FLUENT_UI)
    if(Qt5_VERSION VERSION_LESS "5.14")
        message(STATUS
            "Qt ${Qt5_VERSION} < 5.14：跳过 QWindowKit，两条路线的无边框窗口均不可用")
        # FluentUI 也得一起跳过：它的无边框窗口壳依赖 QWindowKit，没有宿主提供的
        # 目标它就去编自带的那份，而那份会 find_package(qmsetup REQUIRED)——系统里
        # 没有，configure 直接失败。Ubuntu 20.04 自带的 Qt 正好是 5.12，走系统 Qt
        # 的用户必踩（普通变量遮蔽缓存项，不动用户的 cache）
        if(BUILD_FLUENT_UI)
            message(STATUS
                "  同时关闭 BUILD_FLUENT_UI：界面只剩 qss 一条路线")
            set(BUILD_FLUENT_UI OFF)
        endif()
    else()
        set(QWINDOWKIT_BUILD_STATIC ON CACHE BOOL "把 QWindowKit 编成静态库" FORCE)
        set(QWINDOWKIT_BUILD_WIDGETS ON CACHE BOOL "编 QWindowKit 的 widgets 模块" FORCE)
        set(QWINDOWKIT_BUILD_QUICK OFF CACHE BOOL "不编 quick 模块" FORCE)
        set(QWINDOWKIT_BUILD_EXAMPLES OFF CACHE BOOL "不编 QWindowKit 示例" FORCE)
        set(QWINDOWKIT_BUILD_DOCUMENTATIONS OFF CACHE BOOL "不生成文档" FORCE)
        set(QWINDOWKIT_INSTALL OFF CACHE BOOL "不安装 QWindowKit" FORCE)
        add_subdirectory(3rdparty/qwindowkit-1.5.0)

        # 别名由 qwindowkit 无条件创建，这里只是断言：丢了 FluentUIStyle 会
        # 静默改编自带那份，两套 QWK 栈进同一个进程
        if(NOT TARGET QWindowKit::Widgets)
            message(FATAL_ERROR
                "QWindowKit::Widgets 别名不存在：FluentUIStyle 会改编它自带的 "
                "QWindowKit；请检查 3rdparty/qwindowkit-1.5.0 是否被改动")
        endif()
    endif()
endif()

#-------------------------------------------------------------------------------
# SARibbon —— 把主窗口的菜单栏换成功能区（必须排在 QWindowKit 之后）
#-------------------------------------------------------------------------------
if(BUILD_RIBBON)
    # 判据与上游一致：只看 QWindowKit::Widgets 在不在，比自己再判一遍 Qt 版本可靠
    if(TARGET QWindowKit::Widgets)
        set(SARIBBON_USE_FRAMELESS_LIB ON CACHE BOOL "启用无边框窗口" FORCE)
    else()
        message(STATUS
            "没有 QWindowKit 目标：SARibbon 的无边框支持关闭（Ribbon 本体不受影响）")
        set(SARIBBON_USE_FRAMELESS_LIB OFF CACHE BOOL "关闭无边框窗口" FORCE)
    endif()

    # 这三个选项必须在 add_subdirectory 之前设好：SARibbon 用 option() 声明，
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
# 选它不选 FFTW：BSD-3-Clause / Unlicense 对 GPL-2.0，本仓是给人做业务的基础
# 环境，GPL 会把业务代码一起拽进去
#
# 四个 cache 变量必须在 add_subdirectory 之前设好：TEST / TOOLS 默认 ON，且各自
# 带 pkg_check_modules(... REQUIRED ...)（fftw3 / libpng），缺 pkg-config 时
# configure 直接 FATAL；STATIC 默认 OFF，不改会编出要部署的 kissfft-float.dll
#-------------------------------------------------------------------------------
set(KISSFFT_STATIC ON CACHE BOOL "把 kissfft 编成静态库" FORCE)
set(KISSFFT_TEST OFF CACHE BOOL "不编 kissfft 测试（它要 pkg-config 找 fftw3）" FORCE)
set(KISSFFT_TOOLS OFF CACHE BOOL "不编 kissfft 工具（它要 pkg-config 找 libpng）" FORCE)
set(KISSFFT_PKGCONFIG OFF CACHE BOOL "不生成 pkg-config 文件" FORCE)
add_subdirectory(3rdparty/kissfft-131.2.0)

#-------------------------------------------------------------------------------
# FluentUI3Style —— 第二条界面路线（WinUI3/Fluent 风格的 QProxyStyle）
#
# 必须排在 QWindowKit 与 kissfft **之后**：它的 Frameless / ExWidgets 靠
# `if(NOT TARGET ...)` 决定用宿主那份还是自带的，先定义好它自带的副本就不参与
# 构建。样式插件与示例一律不编——插件会往 Qt 安装目录写文件，与自包含相悖
#-------------------------------------------------------------------------------
if(BUILD_FLUENT_UI)
    set(BUILD_PLUGIN OFF CACHE BOOL "不编 Qt 样式插件" FORCE)
    set(BUILD_LIBRARY ON CACHE BOOL "编 FluentUI3Style 本体" FORCE)
    set(BUILD_FRAMELESS ON CACHE BOOL "编 Frameless（QWindowKit 无边框）" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "不编 FluentUIStyle 示例" FORCE)
    set(BUILD_QTCREATOR_PLUGIN OFF CACHE BOOL "不编 Qt Creator 插件" FORCE)
    # 不生成它的 install 规则：① install(EXPORT FramelessTargets) 里的
    # QWKWidgets 没进任何导出集（本仓 QWINDOWKIT_INSTALL=OFF），生成阶段就报错；
    # ② Linux 的 AppImage 走 cmake --install，留着会把它装进 AppDir。变量只在
    # add_subdirectory 调用时被继承，设完就撤，本仓自己的规则不受影响
    set(CMAKE_SKIP_INSTALL_RULES TRUE)
    add_subdirectory(3rdparty/FluentUIStyle-4.0)
    unset(CMAKE_SKIP_INSTALL_RULES)
else()
    message(STATUS "BUILD_FLUENT_UI=OFF，跳过 FluentUI3Style（界面只有 qss 一条路线）")
endif()

#-------------------------------------------------------------------------------
# Markdown 文档预览：qlitehtml（渲染 HTML 的 QWidget）+ litehtml（排版引擎，
# 自带 gumbo）+ md4c（Markdown → HTML）。三个目标由 src/markdown 链接
#
# 位置有硬约束：必须在上面 set(QT_VERSION_MAJOR 5) 与 find_package(Qt5) 之后
# ——qlitehtml 的目标名写作 Qt${QT_VERSION_MAJOR}::Widgets，而这个变量是本仓
# 自己钉的（Qt 5.15 自带的 Qt5Config.cmake 并不定义它），提前加会解析成不存在
# 的 Qt::Widgets
#-------------------------------------------------------------------------------
# litehtml 必须先于 qlitehtml 建出：qlitehtml 的 src/CMakeLists 用
# `if(NOT TARGET litehtml AND EXISTS ...)` 决定要不要建它自带的子模块副本，
# 而官方仓库那份 src/3rdparty/litehtml 是空的（git submodule），本仓单独供
# 0.10 版（已逐文件比对，与子模块快照逐字节相同）
#
# LITEHTML_BUILD_TESTING 默认就是 OFF，不去 set 它：litehtml 的 CMakeLists
# 没设 CMP0077，设了反而报 "option is clearing the normal variable" 的开发期警告
#
# 不生成 litehtml 的 install(EXPORT) 规则：Linux 的 AppImage 走
# cmake --install，留着会把它装进 AppDir（同 FluentUIStyle 的处理）
set(CMAKE_SKIP_INSTALL_RULES TRUE)
add_subdirectory(3rdparty/litehtml-0.10 "${CMAKE_BINARY_DIR}/litehtml")
unset(CMAKE_SKIP_INSTALL_RULES)
# litehtml 对调用方是"上游源码"，头文件标成 SYSTEM 免刷屏
target_include_directories(litehtml SYSTEM INTERFACE
    "${CMAKE_SOURCE_DIR}/3rdparty/litehtml-0.10/include"
    "${CMAKE_SOURCE_DIR}/3rdparty/litehtml-0.10/src")

# 静态：与基座其余第三方一致，免 DLL 部署
set(QLITEHTML_LIBRARY_TYPE STATIC)
# 只加它的 src/：顶层 CMakeLists 会 include(local.cmake) 并建 tests/manual/browser
# （产物名沿用上游的 qlitehtml1.lib，那个 1 取自 project 版本主号）
add_subdirectory(3rdparty/qlitehtml-180e3eb/src "${CMAKE_BINARY_DIR}/qlitehtml")
# AUTOMOC 得手动给它开：上游是在顶层 CMakeLists 里 CMAKE_AUTOMOC ON 的，
# 本仓只加它的 src/，不补这一句 QLiteHtmlWidget 就没有 moc（链接期报一堆
# staticMetaObject / qt_metacall 找不到）
set_target_properties(qlitehtml PROPERTIES AUTOMOC ON)

# md4c 自己列源文件建库，不走它的 CMakeLists：那份会改全局编译设定（覆盖
# CMAKE_BUILD_TYPE、把 C 旗标里的 /MD 清成 /MT）并多建一个 md2html 可执行文件
add_library(md4c STATIC
    "${CMAKE_SOURCE_DIR}/3rdparty/md4c-release-0.5.3/src/md4c.c"
    "${CMAKE_SOURCE_DIR}/3rdparty/md4c-release-0.5.3/src/md4c-html.c"
    "${CMAKE_SOURCE_DIR}/3rdparty/md4c-release-0.5.3/src/entity.c")
target_include_directories(md4c PUBLIC
    "${CMAKE_SOURCE_DIR}/3rdparty/md4c-release-0.5.3/src")
set_target_properties(md4c PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

#-------------------------------------------------------------------------------
# quickjs-ng：文档预览里的公式渲染用它跑 MathJax（tex → svg）
#
# 解压产物不入 git，三处改动（六个命令行工具、qjs-libc 出 ALL、MSVC 下的
# 原子操作 shim）都在 patches/quickjs/ 里，由 cmake/patch_dependencies.cmake
# 在 configure 阶段应用；需要 qjsc/qjs CLI 时显式构建对应目标
#-------------------------------------------------------------------------------
# 关掉它的 install 规则，避免污染系统（同 qlitehtml 的处理）
set(QJS_ENABLE_INSTALL OFF CACHE BOOL "不生成 quickjs-ng 的 install 规则" FORCE)
add_subdirectory(3rdparty/quickjs-0.17.0)

#-------------------------------------------------------------------------------
# 本仓库模块
#-------------------------------------------------------------------------------

# src 下的各模块（core / app）
add_subdirectory(src)

# funq 的手动分离运行示例（两个终端各起一边），依赖上面建出的 demo_app
add_subdirectory(examples)

# 示例自己的部署目标排在 make_deploy 之前：两个 windeployqt 都会往同一个 bin/
# 写文件，并发跑会打架（原因见 examples/fft_demo/CMakeLists.txt）。
# md_viewer 不在这里：它有自己的打包脚本，不参与 -d / -p。
if(TARGET make_deploy AND TARGET deploy_fft_demo)
    add_dependencies(make_deploy deploy_fft_demo)
endif()

# 单元测试
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#-------------------------------------------------------------------------------
# clang 静态分析与格式化脚本
#
# 自带那份（3rdparty/clang）一路优先，不走 find_program 的 HINTS：它只在缓存没
# 结果时才重搜，既有构建目录会一直用着第一次找到的那份（CLAUDE.md 规则 19）。
# 版本固定是目的——clang-format 排版随版本变，混用会让 diff 里全是格式噪声
#-------------------------------------------------------------------------------

if(WIN32)
    # 清掉缓存里的旧搜索结果：下面直接把自带那份写进变量，留着只会误导
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
# 代码统计脚本（cloc）：模板带本机 cloc 路径与源码根，故由 configure_file 产出
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
