# patch_dependencies.cmake
# 对 3rdparty 下解压出来的第三方源码应用补丁。
#
# 为什么用补丁文件而不是直接改解压产物：
#   解压产物不入 git（.gitignore 的 "3rdparty/*/"），直接改会在重解压后丢失，
#   且无法 review。补丁文件入 git（patches/<lib>/NNNN-xxx.patch），可审查、可重放。
#
# 为什么需要 GIT_CEILING_DIRECTORIES：
#   解压产物是被 .gitignore 忽略的未跟踪文件，而 git apply 有一条保护——
#   不修改被忽略的未跟踪文件——会静默 skip 并返回 0（退出码仍是 0，极易误判成功）。
#   把解压目录的父目录设为 ceiling，让 git 认为它不在任何仓库里，即可绕过这层保护。
#
# 幂等：apply 成功后写一个 .patch_applied_<stem> 标记文件；
#       下次 configure 看到标记就跳过。重解压时标记消失，patch 会重放。

set(3RDPARTY_DIR "${CMAKE_SOURCE_DIR}/3rdparty")
set(PATCHES_DIR  "${CMAKE_SOURCE_DIR}/patches")

find_program(GIT_EXECUTABLE NAMES git)
if(NOT GIT_EXECUTABLE)
    message(FATAL_ERROR "git not found; required to apply 3rdparty patches.")
endif()

function(apply_patch_to_dir src_dir patch_file)
    if(NOT EXISTS "${src_dir}")
        message(FATAL_ERROR "Source dir not found: ${src_dir}")
    endif()
    if(NOT EXISTS "${patch_file}")
        message(FATAL_ERROR "Patch file not found: ${patch_file}")
    endif()

    get_filename_component(patch_stem "${patch_file}" NAME_WE)
    set(marker "${src_dir}/.patch_applied_${patch_stem}")
    if(EXISTS "${marker}")
        message(STATUS "  [SKIP] ${patch_stem} (already applied)")
        return()
    endif()

    # 屏蔽仓库上下文，绕过 .gitignore 对未跟踪文件的保护
    get_filename_component(src_parent "${src_dir}" DIRECTORY)
    set(_saved_ceiling "$ENV{GIT_CEILING_DIRECTORIES}")
    set(ENV{GIT_CEILING_DIRECTORIES} "${src_parent}")

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply "${patch_file}"
        WORKING_DIRECTORY "${src_dir}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
        ERROR_VARIABLE  err)

    if(_saved_ceiling)
        set(ENV{GIT_CEILING_DIRECTORIES} "${_saved_ceiling}")
    else()
        unset(ENV{GIT_CEILING_DIRECTORIES})
    endif()

    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "Failed to apply patch:\n"
            "  patch: ${patch_file}\n"
            "  dir  : ${src_dir}\n"
            "  out  : ${out}\n"
            "  err  : ${err}")
    endif()

    file(TOUCH "${marker}")
    message(STATUS "  [OK]   ${patch_stem}")
endfunction()

message(STATUS "")
message(STATUS "Applying 3rdparty patches...")

apply_patch_to_dir("${3RDPARTY_DIR}/QtTesting"
    "${PATCHES_DIR}/QtTesting/0001-qttesting-as-submodule.patch")

message(STATUS "Patches applied.")
message(STATUS "")