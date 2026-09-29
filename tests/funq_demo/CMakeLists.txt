# funq 注入式 UI 测试：靶子是 examples/funq_target。
#
# 与 add_unit_test 那套 gtest 用例不同：被测对象是另一个完整进程，而且要真实
# 窗口与事件循环，所以这里直接 add_test 拉起 python 脚本，由 funq 负责注入与
# 驱动（见 run_funq_tests.py）。
#
# 三个前置缺一不可：靶子可执行文件、funq 的库、以及 Python 解释器。
# 缺任何一个都只跳过并给出提示，不让 ctest 里出现一条注定失败的用例。
#
# 靶子曾用 qt-libui 仓的 gallery 示例：那个示例的控件没有 objectName，路径
# 只能从运行中的进程里探，界面一动就得重探。改用本仓自己的 funq_target 后，
# 控件全带 objectName，路径即 objectName（详见靶子的 main.cpp 注释）。

if(NOT BUILD_FUNQ)
    message(STATUS "BUILD_FUNQ=OFF，跳过 funq 的 UI 测试")
    return()
endif()

find_program(PYTHON_EXECUTABLE NAMES python python3)
if(NOT PYTHON_EXECUTABLE)
    message(WARNING
        "未找到 Python，funq 的 UI 测试不会注册。"
        "funq 需要 Python 3.8+（仅测试需要，编译不需要）")
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
