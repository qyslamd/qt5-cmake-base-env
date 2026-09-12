# check_package_config.cmake - 打包前的构建配置检查
#
# 由 make_package / make_appimage 目标以脚本模式调用（各自打包命令的第一步）：
#   cmake -DBUILD_TYPE=<配置名> [-DPLATFORM=WIN32|LINUX] -P check_package_config.cmake
#
# PLATFORM 可省略，省略时按 WIN32 处理——既有的 Windows 调用方无需改动。
# 两个平台的拒绝/告警理由不同，故分开措辞：Windows 是调试版 C 运行时无法在
# 用户机器启动，Linux 则是体积、断言开销与调试符号的取舍。
#
# 放在构建期而非 configure 期，是为了让 Debug 配置本身仍可正常配置与构建，
# 只在真正要打包时才拦下来。

if(NOT DEFINED BUILD_TYPE OR BUILD_TYPE STREQUAL "")
    return()
endif()

if(NOT DEFINED PLATFORM OR PLATFORM STREQUAL "")
    set(PLATFORM "WIN32")
endif()

if(BUILD_TYPE STREQUAL "Debug")
    if(PLATFORM STREQUAL "WIN32")
        message(FATAL_ERROR
            "拒绝在 Debug 配置下打包。\n"
            "Debug 使用调试版 C 运行时（/MDd），产物依赖 ucrtbased.dll、"
            "vcruntime140d.dll 等，这些不在 VC++ 可再发行包内，用户机器上无法"
            "启动；而开发机装有 Visual Studio，本地测不出该问题。\n"
            "发布请改用: scripts/build_ninja.bat -t relwithdebinfo -p")
    else()
        message(FATAL_ERROR
            "拒绝在 Debug 配置下打包。\n"
            "Debug 产物体积大、带断言检查，而 AppImage 会 strip 掉调试信息，"
            "发布的包既没有性能优势也没有可解析的符号。\n"
            "发布请改用 relwithdebinfo：\n"
            "  cmake --preset ninja-relwithdebinfo-x64\n"
            "  cmake --build --preset ninja-relwithdebinfo-x64-build\n"
            "  cmake --build --preset ninja-relwithdebinfo-x64-build --target make_appimage")
    endif()
endif()

if(NOT BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(PLATFORM STREQUAL "WIN32")
        message(WARNING
            "${BUILD_TYPE} 配置不产出可用的调试符号，一旦安装包出问题将无法解析"
            "崩溃转储。发布建议使用 relwithdebinfo 配置。")
    else()
        message(WARNING
            "${BUILD_TYPE} 配置不产出调试符号，归档的可执行文件无法解析崩溃日志里"
            "的地址。发布建议使用 relwithdebinfo 配置。")
    endif()
endif()
