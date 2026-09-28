# cmake/crashpad/crashpad.cmake
#
# Crashpad 一站式引入。由顶层 CMakeLists 在 APP_ENABLE_CRASHPAD=ON 时 include，
# 不进子项目、不改全局输出目录、不引 crashpad 自带 examples 与单测。
#
# 依赖位置：
#   ${CMAKE_SOURCE_DIR}/3rdparty/crashpad-7b9686b
#   ${CMAKE_SOURCE_DIR}/3rdparty/mini_chromium-9cdc2a7
#   ${CMAKE_SOURCE_DIR}/3rdparty/zlib-v1.2.11
#   ${CMAKE_SOURCE_DIR}/3rdparty/linux-syscall-support-e1e7b0a

#------------------------------------------------------------------------------
# 路径变量（子模块靠这两个引用源码）
#------------------------------------------------------------------------------
set(crashpad_git_SOURCE_DIR      "${CMAKE_SOURCE_DIR}/3rdparty/crashpad-7b9686b")
set(mini_chromium_git_SOURCE_DIR "${CMAKE_SOURCE_DIR}/3rdparty/mini_chromium-9cdc2a7")
set(lss_git_SOURCE_DIR           "${CMAKE_SOURCE_DIR}/3rdparty/linux-syscall-support-e1e7b0a")

if(NOT EXISTS "${crashpad_git_SOURCE_DIR}")
    message(FATAL_ERROR "Crashpad 源码未解压：${crashpad_git_SOURCE_DIR}")
endif()
if(NOT EXISTS "${mini_chromium_git_SOURCE_DIR}")
    message(FATAL_ERROR "mini_chromium 源码未解压：${mini_chromium_git_SOURCE_DIR}")
endif()
if(NOT EXISTS "${lss_git_SOURCE_DIR}")
    message(FATAL_ERROR "LSS 源码未解压：${lss_git_SOURCE_DIR}")
endif()

#------------------------------------------------------------------------------
# 模块搜索路径：让 crashpad-*.cmake / minichromium.cmake 能被 include 到
#------------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

#------------------------------------------------------------------------------
# ASM 语言（crashpad util 在 Windows 下用 .asm 写 capture_context / safe_terminate）
#------------------------------------------------------------------------------
if(MSVC)
    if(NOT CMAKE_ASM_MASM_COMPILER_LOADED)
        enable_language(ASM_MASM)
    endif()
else()
    if(NOT CMAKE_ASM_COMPILER_LOADED)
        enable_language(ASM)
    endif()
endif()

#------------------------------------------------------------------------------
# 线程库（非 Windows）
#------------------------------------------------------------------------------
if(NOT WIN32)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()

#------------------------------------------------------------------------------
# zlib —— 若其他库已定义 ZLIB::ZLIB 则复用，不重复编译
#------------------------------------------------------------------------------
if(NOT TARGET ZLIB::ZLIB)
    add_subdirectory(
        "${CMAKE_SOURCE_DIR}/3rdparty/zlib-v1.2.11"
        "${CMAKE_BINARY_DIR}/crashpad/zlib"
        EXCLUDE_FROM_ALL
    )
    target_include_directories(zlibstatic INTERFACE
        "${CMAKE_SOURCE_DIR}/3rdparty/zlib-v1.2.11"
        "${CMAKE_BINARY_DIR}/crashpad/zlib"
    )
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

#------------------------------------------------------------------------------
# AppleFrameworks 空壳。crashpad 各库都引用它，非 APPLE 平台展开为空
#------------------------------------------------------------------------------
if(NOT TARGET AppleFrameworks)
    add_library(AppleFrameworks INTERFACE)
endif()

#------------------------------------------------------------------------------
# LSS 头文件：crashpad util 的 Linux 部分会 #include "third_party/lss/..."
# Windows 下用不到，但复制一次无害（file(COPY) 是幂等的）
#------------------------------------------------------------------------------
if(UNIX AND NOT APPLE)
    file(COPY "${lss_git_SOURCE_DIR}/linux_syscall_support.h"
         DESTINATION "${crashpad_git_SOURCE_DIR}/third_party/lss")
endif()

#------------------------------------------------------------------------------
# 库定义（顺序有依赖，保持如下顺序）
#------------------------------------------------------------------------------
include(minichromium)
include(crashpad-common)
include(crashpad-compat)
include(crashpad-tools)
include(crashpad-util)
include(crashpad-client)
include(crashpad-minidump)
include(crashpad-snapshot)
include(crashpad-handler)

#------------------------------------------------------------------------------
# 引入结果摘要
#------------------------------------------------------------------------------
message(STATUS "Crashpad 已引入：")
message(STATUS "  源码根             : ${crashpad_git_SOURCE_DIR}")
message(STATUS "  mini_chromium 根   : ${mini_chromium_git_SOURCE_DIR}")
message(STATUS "  crashpad_handler   : ${CMAKE_BINARY_DIR}/crashpad_handler")