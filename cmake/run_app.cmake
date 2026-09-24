# run_app.cmake - 运行脚本生成宏
# 封装 configure_file + chmod，统一生成 Windows(.bat) / Linux(.sh) 运行脚本。

# configure_run_script(<output_basename> [ARGS "<默认命令行参数>"])
#   output_basename: 输出文件名（不含扩展名），如 "run_app" / "run_${TARGET_NAME}"
#   ARGS:            可选；烘焙进脚本的默认命令行参数（原样替换，含引号需
#                    自带转义）。脚本同时透传调用时附加的参数：
#                    .bat 追加 %*，.sh 追加 "$@"
#
# 依赖调用方作用域中的 TARGET_NAME（供模板 @TARGET_NAME@ 替换）。
# 脚本输出到 CMAKE_RUNTIME_OUTPUT_DIRECTORY，即该目标可执行文件所在目录，
# 因此测试目录把它改到 bin/tests 后无需额外传参。
macro(configure_run_script output_basename)
    cmake_parse_arguments(CRS "" "ARGS" "" ${ARGN})

    if(WIN32)
        set(_template "${CMAKE_SOURCE_DIR}/cmake/template/run_app.bat.in")
        set(_output "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${output_basename}.bat")
        # cmd 的 cd 不认正斜杠路径，bat 内必须用原生反斜杠形式
        file(TO_NATIVE_PATH "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" RUN_DIR)
    elseif(UNIX)
        set(_template "${CMAKE_SOURCE_DIR}/cmake/run_app.sh.in")
        set(_output "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${output_basename}.sh")
        set(RUN_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif()

    if(_template)
        set(PROGRAM_ARGS "${CRS_ARGS}")
        configure_file("${_template}" "${_output}" @ONLY)
        # Linux 下 .sh 需可执行权限
        if(UNIX)
            file(CHMOD "${_output}"
                PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_EXECUTE
                            WORLD_READ WORLD_EXECUTE)
        endif()
    endif()

    unset(PROGRAM_ARGS)
    unset(RUN_DIR)
    unset(_template)
    unset(_output)
endmacro()
