# funq_python.cmake —— funq 用的 Python 解释器：**位数必须与目标架构一致**。
#
# 为什么这条要单独成文件：funq 的注入是「Python 进程调 CreateRemoteThread +
# LoadLibrary」，而 Windows 不允许跨位数注入。错配的表现极具误导性——不报错，
# 只表现为注入失败、然后一路等到超时（tests/funq_demo 那边曾经是 180 秒）。
# tests/funq_demo 与 examples/funq-scripts 两处都要按同一条规则挑解释器，
# 规则只留在这里一份（示例这边当初漏了，挑到 64 位 Python 对 x86 目标）。
#
# 选择顺序：仓库自带的那份（按目标架构挑，3.8.8）→ -DFUNQ_PYTHON 指定的 →
# PATH 上的。自带那份优先，是为了不依赖「这台机器上装了什么」：版本与位数都由
# 仓库定死。两份解释器在解压阶段就位（见 cmake/extract_dependencies.cmake），
# 并随安装包一起发（见 src/app/CMakeLists.txt 的 make_funq_kit）。
#
# 用法：funq_find_python(<输出变量> [<用途说明>])
#   成功：<输出变量> = 解释器路径（位数已核对）
#   失败：<输出变量> = 空串，且已打印说明原因的 WARNING，调用方直接 return() 即可

if(NOT COMMAND funq_find_python)

    set(FUNQ_PYTHON "" CACHE FILEPATH
        "funq 用的 Python 解释器（留空则用仓库自带的那份，再退回 PATH；位数须与目标架构一致）")

    function(funq_find_python out_var)
        set(purpose "${ARGV1}")
        if(NOT purpose)
            set(purpose "funq")
        endif()

        # 仓库自带的那份：按目标架构二选一
        set(bundled "")
        if(WIN32)
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(bundled
                    "${CMAKE_SOURCE_DIR}/3rdparty/python-3.8.8-embed-amd64/python.exe")
            else()
                set(bundled
                    "${CMAKE_SOURCE_DIR}/3rdparty/python-3.8.8-embed-win32/python.exe")
            endif()
        endif()

        if(FUNQ_PYTHON)
            set(python "${FUNQ_PYTHON}")
        elseif(bundled AND EXISTS "${bundled}")
            set(python "${bundled}")
            message(STATUS "${purpose}用仓库自带的 Python: ${python}")
        else()
            find_program(python NAMES python python3)
        endif()

        if(NOT python)
            message(WARNING
                "未找到 Python，${purpose}不会生成/注册。"
                "funq 需要 Python 3.8+（仅运行需要，编译不需要）")
            set(${out_var} "" PARENT_SCOPE)
            return()
        endif()

        # 位数核对：让候选解释器自己报一次指针宽度，与目标架构比对
        execute_process(COMMAND "${python}" -c
                            "import struct; print(struct.calcsize('P') * 8)"
            OUTPUT_VARIABLE python_bits
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE probe_result)
        if(NOT probe_result EQUAL 0)
            message(WARNING
                "${python} 跑不起来（探针返回 ${probe_result}），${purpose}不会"
                "生成/注册。可用 -DFUNQ_PYTHON=<路径> 另指定一个")
            set(${out_var} "" PARENT_SCOPE)
            return()
        endif()

        set(target_bits 32)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(target_bits 64)
        endif()
        if(NOT python_bits STREQUAL "${target_bits}")
            message(WARNING
                "Python 是 ${python_bits} 位而目标是 ${target_bits} 位，${purpose}"
                "不会生成/注册——注入要跨位数，Windows 不允许，错配只会以注入"
                "失败或超时的形式表现出来。"
                "用 -DFUNQ_PYTHON=<${target_bits} 位 python 的路径> 指定一个匹配的")
            set(${out_var} "" PARENT_SCOPE)
            return()
        endif()

        set(${out_var} "${python}" PARENT_SCOPE)
    endfunction()

endif()
