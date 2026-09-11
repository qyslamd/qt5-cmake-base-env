# archive_symbols.cmake - 归档发布符号
#
# 由 make_symbols 目标以脚本模式调用：
#   cmake -DSRC_EXE=<exe> -DSRC_PDB=<pdb> -DDEST_DIR=<dir> -P archive_symbols.cmake
#
# 之所以用脚本而非直接堆 add_custom_command：未启用调试信息的配置
# （如 release）不会生成 PDB，此时应告警而非让打包直接失败。

if(NOT DEFINED SRC_EXE OR SRC_EXE STREQUAL "")
    message(FATAL_ERROR "SRC_EXE 未指定")
endif()
if(NOT DEFINED DEST_DIR OR DEST_DIR STREQUAL "")
    message(FATAL_ERROR "DEST_DIR 未指定")
endif()

file(MAKE_DIRECTORY "${DEST_DIR}")

if(NOT EXISTS "${SRC_EXE}")
    message(FATAL_ERROR "可执行文件不存在: ${SRC_EXE}")
endif()
file(COPY "${SRC_EXE}" DESTINATION "${DEST_DIR}")
message(STATUS "已归档可执行文件: ${SRC_EXE}")

# SRC_PDB 可能为空（非 MSVC）或指向不存在的文件（release 配置）
if(DEFINED SRC_PDB AND NOT SRC_PDB STREQUAL "" AND EXISTS "${SRC_PDB}")
    file(COPY "${SRC_PDB}" DESTINATION "${DEST_DIR}")
    message(STATUS "已归档符号文件: ${SRC_PDB}")
else()
    message(WARNING
        "未找到符号文件（${SRC_PDB}），本次不归档符号。"
        "该配置未启用调试信息，其崩溃转储将无法解析调用栈；"
        "发布请使用 relwithdebinfo 配置。")
endif()
