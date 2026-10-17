# 测试公共配置与 add_unit_test 宏定义在 cmake/test_helpers.cmake，
# 由本文件引入后各测试子目录直接使用
include(${CMAKE_SOURCE_DIR}/cmake/test_helpers.cmake)

# ============== 子目录注册 ==============

add_subdirectory(core)
add_subdirectory(logging)
add_subdirectory(app)
add_subdirectory(tour)
add_subdirectory(markdown)
add_subdirectory(quickjs)
add_subdirectory(funq_demo)
add_subdirectory(funq_app)

# ============== Python 脚本的静态检查 ==============
#
# 覆盖 examples 下那几份 funq 脚本和 tests/funq_* 里驱动 funq 的夹具。它们都
# 没有别的自动检查——上面那几条 funq 用例验的是库与靶子，不跑示例脚本；夹具
# 本身更是只有真去跑一遍才会暴露问题。产品线上因此烂过一次：演示脚本里出现
# 两个同名的顶层类，后一个悄悄盖掉前一个，一跑就抛 AttributeError，而 ctest
# 全绿。这类"重定义 / 未定义名"外加"改名之后调用方没跟上"，都是 pyflakes 一眼
# 能看出来的——而它们在 funq 这里的表现往往是"找不到控件"，离真正的原因很远。
#
# 解释器用 CMake 找到的系统 python，**不是** funq_find_python 那个按目标位数
# 挑的（那可能是随包的 3.8 embed，装不了 pyflakes）。没装 pyflakes 就跳过并
# 给出提示，不让 ctest 里出现一条注定失败的用例——与 funq 用例同一条规矩。
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -c "import pyflakes"
        RESULT_VARIABLE FUNQ_PYFLAKES_RC
        OUTPUT_QUIET ERROR_QUIET)
    if(FUNQ_PYFLAKES_RC EQUAL 0)
        # CONFIGURE_DEPENDS：glob 是配置期求值的，不带它就得"新加一个 .py
        # 之后重新 configure 才会被检查"——那正是最容易被忘掉的一步
        file(GLOB FUNQ_SCRIPT_FILES CONFIGURE_DEPENDS
            "${CMAKE_SOURCE_DIR}/examples/funq-scripts/*.py"
            "${CMAKE_SOURCE_DIR}/tests/funq_demo/*.py"
            "${CMAKE_SOURCE_DIR}/tests/funq_app/*.py")
        add_test(NAME test_funq_scripts
            COMMAND "${Python3_EXECUTABLE}" -m pyflakes ${FUNQ_SCRIPT_FILES})
        set_tests_properties(test_funq_scripts PROPERTIES LABELS "SCRIPT")
    else()
        message(STATUS
            "未安装 pyflakes，跳过 funq 脚本的静态检查"
            "（pip install --user pyflakes 后重新配置即可）")
    endif()
endif()
