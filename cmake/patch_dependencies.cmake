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

    # core.autocrlf=false：git apply 默认按开发者本机的 autocrlf 决定写盘
    # 行尾（实测 autocrlf=true 时会把被补丁修改的整个文件写成 CRLF），
    # 这会让产物行尾随各人配置漂移。补丁钉死 LF，这里也要钉死
    # --ignore-whitespace：补丁在 .gitattributes 里钉死为 LF，而个别上游源码是
    # CRLF（SARibbon 的 CMakeLists 就是），行尾不一致会让 git apply 按行匹配失败
    # 直接报 "patch does not apply"。实测该开关可让 LF 补丁落到 CRLF 目标上。
    # 代价：补丁新增/修改的那些行按补丁的行尾（LF）写盘，目标文件因此成为
    # 混合行尾——CMake 两者都认，且该文件是解压产物、每次解压重新生成，可接受
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -c core.autocrlf=false apply --ignore-whitespace
                "${patch_file}"
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

apply_patch_to_dir("${3RDPARTY_DIR}/funq-master"
    "${PATCHES_DIR}/funq/0001-funq-as-subproject.patch")
apply_patch_to_dir("${3RDPARTY_DIR}/funq-master"
    "${PATCHES_DIR}/funq/0002-qt5-only.patch")
apply_patch_to_dir("${3RDPARTY_DIR}/funq-master"
    "${PATCHES_DIR}/funq/0003-msvc-warnings.patch")
apply_patch_to_dir("${3RDPARTY_DIR}/funq-master"
    "${PATCHES_DIR}/funq/0004-drop-nose-plugin.patch")
apply_patch_to_dir("${3RDPARTY_DIR}/funq-master"
    "${PATCHES_DIR}/funq/0005-alias-file-encoding.patch")
apply_patch_to_dir("${3RDPARTY_DIR}/funq-master"
    "${PATCHES_DIR}/funq/0006-drop-qtquick-component.patch")
apply_patch_to_dir("${3RDPARTY_DIR}/funq-master"
    "${PATCHES_DIR}/funq/0007-runner-library-argument.patch")
apply_patch_to_dir("${3RDPARTY_DIR}/funq-master"
    "${PATCHES_DIR}/funq/0008-pick-output-through-qt-logging.patch")

apply_patch_to_dir("${3RDPARTY_DIR}/QXlsx-1.5.0"
    "${PATCHES_DIR}/QXlsx/0001-qt5-only.patch")

apply_patch_to_dir("${3RDPARTY_DIR}/SARibbon-2.5.7"
    "${PATCHES_DIR}/SARibbon/0001-qt5-only.patch")
apply_patch_to_dir("${3RDPARTY_DIR}/SARibbon-2.5.7"
    "${PATCHES_DIR}/SARibbon/0002-bump-cmake-minimum-version.patch")
apply_patch_to_dir("${3RDPARTY_DIR}/SARibbon-2.5.7"
    "${PATCHES_DIR}/SARibbon/0003-frameless-as-in-tree-target.patch")
apply_patch_to_dir("${3RDPARTY_DIR}/SARibbon-2.5.7"
    "${PATCHES_DIR}/SARibbon/0004-drop-install-export.patch")

apply_patch_to_dir("${3RDPARTY_DIR}/qwindowkit-1.5.0"
    "${PATCHES_DIR}/QWindowKit/0001-qmsetup-source-dir.patch")

apply_patch_to_dir("${3RDPARTY_DIR}/zlib-v1.2.11"
    "${PATCHES_DIR}/zlib/0001-bump-cmake-minimum-version.patch")

message(STATUS "Patches applied.")
message(STATUS "")