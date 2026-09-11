# check_package_config.cmake - 打包前的构建配置检查
#
# 由 make_package 目标以脚本模式调用（该目标的第一条命令，先于 ISCC）：
#   cmake -DBUILD_TYPE=<配置名> -P check_package_config.cmake
#
# 放在构建期而非 configure 期，是为了让 Debug 配置本身仍可正常配置与构建，
# 只在真正要打包时才拦下来。

if(NOT DEFINED BUILD_TYPE OR BUILD_TYPE STREQUAL "")
    return()
endif()

if(BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR
        "拒绝在 Debug 配置下打包。\n"
        "Debug 使用调试版 C 运行时（/MDd），产物依赖 ucrtbased.dll、"
        "vcruntime140d.dll 等，这些不在 VC++ 可再发行包内，用户机器上无法"
        "启动；而开发机装有 Visual Studio，本地测不出该问题。\n"
        "发布请改用: scripts/build_ninja.bat -t relwithdebinfo -p")
endif()

if(NOT BUILD_TYPE STREQUAL "RelWithDebInfo")
    message(WARNING
        "${BUILD_TYPE} 配置不产出可用的调试符号，一旦安装包出问题将无法解析"
        "崩溃转储。发布建议使用 relwithdebinfo 配置。")
endif()
