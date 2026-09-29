# funq 注入式 UI 测试：靶子是 examples/funq_target。
#
# 与 add_unit_test 那套 gtest 用例不同：被测对象是另一个完整进程，而且要真实
# 窗口与事件循环，所以这里直接 add_test 拉起 python 脚本，由 funq 负责注入与
# 驱动（见 run_funq_tests.py）。
#
# 四个前置缺一不可：靶子可执行文件、funq 的库、Python 解释器，以及**解释器的
# 位数与目标架构一致**。缺任何一个都只跳过并给出提示，不让 ctest 里出现一条注定
# 失败的用例——位数那条尤其要紧：不匹配时用例不报错，而是每个方法都 ERROR、
# 然后挂到 ctest 的超时（实测 180 秒），看起来像卡死。
#
# 靶子曾用 qt-libui 仓的 gallery 示例：那个示例的控件没有 objectName，路径
# 只能从运行中的进程里探，界面一动就得重探。改用本仓自己的 funq_target 后，
# 控件全带 objectName，路径即 objectName（详见靶子的 main.cpp 注释）。

if(NOT BUILD_FUNQ)
    message(STATUS "BUILD_FUNQ=OFF，跳过 funq 的 UI 测试")
    return()
endif()

# 解释器按这个顺序找：**仓库自带的那份**（按目标架构挑）→ -DFUNQ_PYTHON 指定的
# → PATH 上的。
#
# 自带那份优先，为的是让测试不依赖"这台机器上装了什么"：版本（3.8.8）与位数都由
# 仓库定死，也就不会再出现"64 位 Python 注不进 32 位靶子"那类只有超时能看出来的
# 问题——注入是 Python 进程在调 CreateRemoteThread + LoadLibrary，跨位数是不允许的。
# 两份的解释器在解压阶段就位（见 cmake/extract_dependencies.cmake），且随安装包
# 一起发（见 src/app/CMakeLists.txt 的 make_funq_kit）
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(FUNQ_BUNDLED_PYTHON
            "${CMAKE_SOURCE_DIR}/3rdparty/python-3.8.8-embed-amd64/python.exe")
    else()
        set(FUNQ_BUNDLED_PYTHON
            "${CMAKE_SOURCE_DIR}/3rdparty/python-3.8.8-embed-win32/python.exe")
    endif()
endif()

set(FUNQ_PYTHON "" CACHE FILEPATH
    "funq UI 测试用的 Python 解释器（留空则用仓库自带的那份，再退回 PATH；位数须与目标架构一致）")

if(FUNQ_PYTHON)
    set(PYTHON_EXECUTABLE "${FUNQ_PYTHON}")
elseif(FUNQ_BUNDLED_PYTHON AND EXISTS "${FUNQ_BUNDLED_PYTHON}")
    set(PYTHON_EXECUTABLE "${FUNQ_BUNDLED_PYTHON}")
    message(STATUS "funq 的 UI 测试用仓库自带的 Python: ${PYTHON_EXECUTABLE}")
else()
    find_program(PYTHON_EXECUTABLE NAMES python python3)
endif()
if(NOT PYTHON_EXECUTABLE)
    message(WARNING
        "未找到 Python，funq 的 UI 测试不会注册。"
        "funq 需要 Python 3.8+（仅测试需要，编译不需要）")
    return()
endif()

# 位数核对：让候选解释器自己报一次指针宽度，与目标架构比对
execute_process(COMMAND "${PYTHON_EXECUTABLE}" -c
                    "import struct; print(struct.calcsize('P') * 8)"
    OUTPUT_VARIABLE FUNQ_PYTHON_BITS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE FUNQ_PYTHON_PROBE_RESULT)
if(NOT FUNQ_PYTHON_PROBE_RESULT EQUAL 0)
    message(WARNING
        "${PYTHON_EXECUTABLE} 跑不起来（探针返回 ${FUNQ_PYTHON_PROBE_RESULT}），"
        "funq 的 UI 测试不会注册。可用 -DFUNQ_PYTHON=<路径> 另指定一个")
    return()
endif()

set(FUNQ_TARGET_BITS 32)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(FUNQ_TARGET_BITS 64)
endif()
if(NOT FUNQ_PYTHON_BITS STREQUAL "${FUNQ_TARGET_BITS}")
    message(WARNING
        "Python 是 ${FUNQ_PYTHON_BITS} 位而目标是 ${FUNQ_TARGET_BITS} 位，"
        "funq 的 UI 测试不会注册——注入要跨位数，Windows 不允许，"
        "而这里不注册它就会以 180 秒超时的形式表现出来。"
        "用 -DFUNQ_PYTHON=<${FUNQ_TARGET_BITS} 位 python 的路径> 指定一个匹配的")
    return()
endif()

if(NOT TARGET funq_target)
    message(STATUS "未构建 funq_target 目标，跳过 funq 的 UI 测试")
    return()
endif()

# 靶子的输出目录由顶层统一到 <build>/bin。
# 这里必须用普通路径而不是 $<TARGET_FILE:...>：funq.conf 由 configure_file
# 生成，而它不求值生成器表达式——genex 会被原样写进配置，funq 随后拿这个
# 字符串当可执行文件路径去拉起，报"连接被拒绝"（实测踩过）
set(TARGET_EXE "${CMAKE_BINARY_DIR}/bin/funq_target${CMAKE_EXECUTABLE_SUFFIX}")
set(TARGET_DIR "${CMAKE_BINARY_DIR}/bin")
# 别名表入库（放源码树）
set(TARGET_ALIAS "${CMAKE_CURRENT_SOURCE_DIR}/funq_target.alias")

set(FUNQ_CLIENT_DIR "${CMAKE_SOURCE_DIR}/3rdparty/funq-master/client")
set(FUNQ_SERVER_DIR "${CMAKE_SOURCE_DIR}/3rdparty/funq-master/server")

configure_file(funq.conf.in "${CMAKE_CURRENT_BINARY_DIR}/funq.conf" @ONLY)

# 注入器包装脚本：Windows 是 .bat，Linux 是 .sh（内容都是「转交给 funq
# runner」，见各自的 .in）。funq 客户端把它当可执行文件直接 exec，所以
# .sh 必须带 +x 位——Windows 的 .bat 靠扩展名关联，没这一步
if(WIN32)
    set(FUNQ_ATTACH_NAME funq_attach.bat)
    configure_file(funq_attach.bat.in
        "${CMAKE_CURRENT_BINARY_DIR}/${FUNQ_ATTACH_NAME}" @ONLY)
else()
    set(FUNQ_ATTACH_NAME funq_attach.sh)
    configure_file(funq_attach.sh.in
        "${CMAKE_CURRENT_BINARY_DIR}/${FUNQ_ATTACH_NAME}" @ONLY)
    file(CHMOD "${CMAKE_CURRENT_BINARY_DIR}/${FUNQ_ATTACH_NAME}"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE)
endif()

add_test(NAME test_funq_demo
    COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_funq_tests.py")

# 每次用例都要拉起应用并等注入完成（funq 在启动后固定 sleep 1 秒再注入），
# 10 秒的默认超时不够用；用例数与注入延迟叠加后目前约 10 秒，留足余量。
#
# 环境变量必须先汇成一个列表变量再整体传入：set_tests_properties 的
# PROPERTIES 是「名 值」成对解析的，把多个 VAR=value 作为并列参数传入时，
# 只有第一个会被当作 ENVIRONMENT 的值，其余会被当成属性名。
#
# Qt 的 bin 只传目录进来，由脚本自己拼进 PATH：PATH 本身就是分号分隔，直接
# 写进 ENVIRONMENT 会被拆散
set(FUNQ_TEST_ENV
    "FUNQ_CONF=${CMAKE_CURRENT_BINARY_DIR}/funq.conf"
    "FUNQ_ATTACH_EXE=${CMAKE_CURRENT_BINARY_DIR}/${FUNQ_ATTACH_NAME}"
    "FUNQ_CLIENT_DIR=${FUNQ_CLIENT_DIR}"
    "FUNQ_SERVER_DIR=${FUNQ_SERVER_DIR}"
    "FUNQ_QT_BIN=${QT_BIN_DIR}")

set_tests_properties(test_funq_demo PROPERTIES
    TIMEOUT 180
    LABELS "APP;FUNQ"
    ENVIRONMENT "${FUNQ_TEST_ENV}")
