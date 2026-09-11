# version_manage.cmake - 版本号解析
#
# 版本号由 git 派生，而非自增文件：
#   major.minor.patch  最近的 tag（v1.2.3 形式）
#   tweak              提交总数，随提交单调递增
# 因此同一个提交在任何机器上构建都得到同样的版本号，且版本号可反查提交。
#
# 无 git 环境（如导出的源码包）时回落 0.0.0.0 并告警；
# 需要固定版本号时用 -DAPP_VERSION_OVERRIDE=major.minor.patch.tweak 指定。

set(APP_VERSION_OVERRIDE "" CACHE STRING
    "覆盖自动探测的版本号，格式 major.minor.patch.tweak")

# resolve_app_version(OUTPUT_PREFIX)
#   输出变量：<PREFIX>_VERSION_MAJOR / _MINOR / _PATCH / _TWEAK
#             <PREFIX>_VERSION 完整四段式，如 1.2.3.42
#             <PREFIX>_GIT_HASH 短 hash，工作区有改动时带 -dirty 后缀
function(resolve_app_version OUTPUT_PREFIX)
    set(_major 0)
    set(_minor 0)
    set(_patch 0)
    set(_tweak 0)
    set(_hash "unknown")

    if(APP_VERSION_OVERRIDE)
        if(APP_VERSION_OVERRIDE MATCHES
           "^([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
            set(_major ${CMAKE_MATCH_1})
            set(_minor ${CMAKE_MATCH_2})
            set(_patch ${CMAKE_MATCH_3})
            set(_tweak ${CMAKE_MATCH_4})
            message(STATUS "版本号由 APP_VERSION_OVERRIDE 指定: "
                "${APP_VERSION_OVERRIDE}")
        else()
            message(FATAL_ERROR
                "APP_VERSION_OVERRIDE 格式错误，应为 major.minor.patch.tweak")
        endif()
    else()
        _resolve_version_from_git(_major _minor _patch _tweak _hash)
    endif()

    set(${OUTPUT_PREFIX}_VERSION_MAJOR ${_major} PARENT_SCOPE)
    set(${OUTPUT_PREFIX}_VERSION_MINOR ${_minor} PARENT_SCOPE)
    set(${OUTPUT_PREFIX}_VERSION_PATCH ${_patch} PARENT_SCOPE)
    set(${OUTPUT_PREFIX}_VERSION_TWEAK ${_tweak} PARENT_SCOPE)
    set(${OUTPUT_PREFIX}_VERSION
        "${_major}.${_minor}.${_patch}.${_tweak}" PARENT_SCOPE)
    set(${OUTPUT_PREFIX}_GIT_HASH "${_hash}" PARENT_SCOPE)
endfunction()

# 从 git 读取版本，结果通过同名变量名传出
macro(_resolve_version_from_git out_major out_minor out_patch out_tweak
        out_hash)
    # 最近的版本 tag：v1.2.3 -> 1.2.3；无 tag 则停留在 0.0.0
    execute_process(
        COMMAND git describe --tags --abbrev=0 --match "v*"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE _tag
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _rc_tag)
    if(_rc_tag EQUAL 0 AND _tag MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
        set(${out_major} ${CMAKE_MATCH_1})
        set(${out_minor} ${CMAKE_MATCH_2})
        set(${out_patch} ${CMAKE_MATCH_3})
    endif()

    # 提交总数作为 build 号
    execute_process(
        COMMAND git rev-list --count HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE _count
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _rc_count)
    if(_rc_count EQUAL 0 AND _count MATCHES "^[0-9]+$")
        set(${out_tweak} ${_count})
    else()
        message(WARNING
            "未能从 git 读取版本信息（可能不在 git 仓库中，或尚无提交）。"
            "版本号回落 0.0.0.0；需要固定版本号时用 "
            "-DAPP_VERSION_OVERRIDE=major.minor.patch.tweak 指定。")
        return()
    endif()

    # 短 hash；工作区有未提交改动时打 -dirty 标记
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE _short
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    execute_process(
        COMMAND git status --porcelain
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE _dirty
        ERROR_QUIET)
    if(_short MATCHES "^[0-9a-f]+$")
        set(${out_hash} "${_short}")
        if(_dirty)
            set(${out_hash} "${_short}-dirty")
        endif()
    endif()
endmacro()
