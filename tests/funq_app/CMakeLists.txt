# funq 注入式 UI 测试：靶子就是本仓真程序 demo_app。
#
# 与 tests/funq_demo 的分工：那边的靶子是 examples/funq_target，验的是 funq
# 本身能不能用（控件全带 objectName、界面刻意做得简单）；这里的靶子是**真
# 程序**——登录、单实例、启动顺序这些东西只有拿真程序当靶子才验得到。
#
# 与 add_unit_test 那套 gtest 用例不同：被测对象是另一个完整进程，而且要真实
# 窗口与事件循环，所以这里直接 add_test 拉起 python 脚本，由 funq 负责注入与
# 驱动（见 run_funq_tests.py）。
#
# 四个前置缺一不可：靶子可执行文件、funq 的库、Python 解释器，以及**解释器的
# 位数与目标架构一致**。缺任何一个都只跳过并给出提示，不让 ctest 里出现一条
# 注定失败的用例——位数那条尤其要紧：不匹配时用例不报错，而是每个方法都
# ERROR、然后挂到 ctest 的超时（实测 180 秒），看起来像卡死。
#
# 靶子是真程序的代价：它比示例重（要读配置），而启动路径上任何一步缺失都
# 表现成"主窗口不出现"，见 funq_app.conf.in 里那段注释（登录与崩溃报告提示
# 已由 args 的 --set 关掉）。

if(NOT BUILD_FUNQ)
    message(STATUS "BUILD_FUNQ=OFF，跳过 funq 的 UI 测试")
    return()
endif()

# 解释器必须与目标架构同位数——规则、理由与选择顺序见 cmake/funq_python.cmake。
# 可用 -DFUNQ_PYTHON=<路径> 覆盖
include(${CMAKE_SOURCE_DIR}/cmake/funq_python.cmake)
funq_find_python(PYTHON_EXECUTABLE "funq 的 UI 测试")
if(NOT PYTHON_EXECUTABLE)
    return()
endif()

if(NOT TARGET ${APP_TARGET_PREFIX}_app)
    message(STATUS "未构建 ${APP_TARGET_PREFIX}_app 目标，跳过 funq 的 UI 测试")
    return()
endif()

# 靶子的输出目录由顶层统一到 <build>/bin。
# 这里必须用普通路径而不是 $<TARGET_FILE:...>：funq.conf 由 configure_file
# 生成，而它不求值生成器表达式——genex 会被原样写进配置，funq 随后拿这个
# 字符串当可执行文件路径去拉起，报"连接被拒绝"（实测踩过）
set(TARGET_EXE
    "${CMAKE_BINARY_DIR}/bin/${APP_TARGET_PREFIX}_app${CMAKE_EXECUTABLE_SUFFIX}")
set(TARGET_DIR "${CMAKE_BINARY_DIR}/bin")
# 别名表入库（放源码树）
set(TARGET_ALIAS "${CMAKE_CURRENT_SOURCE_DIR}/demo_app.alias")

set(FUNQ_CLIENT_DIR "${CMAKE_SOURCE_DIR}/3rdparty/funq-master/client")
set(FUNQ_SERVER_DIR "${CMAKE_SOURCE_DIR}/3rdparty/funq-master/server")

configure_file(funq_app.conf.in "${CMAKE_CURRENT_BINARY_DIR}/funq.conf" @ONLY)

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

# 这条用例超时、报 "Unable to find widget with path `MainWindow`" 时，先查有
# 没有残留进程，别先怀疑用例或界面。
#
# 症状：一轮跑到一半出问题的运行可能留下一个孤儿进程，而它占着单实例互斥体。
# 下一次运行拉起的那个第二实例只会把命令行转交给孤儿（见 src/app/main.cpp 的
# HandleAlreadyRunning）然后自己退出，窗口根本不开——funq 驱动的其实是一个
# 已经消失的进程，报出来的还是同一句 MainWindow 路径错误。
#
# 为什么 funq 不收拾它：_kill_process（client/funq/client.py）杀的是它自己起
# 的那个进程，也就是本目录的 funq_attach.bat（cmd.exe）；应用是孙子进程，活
# 下来。处置：重跑前把残留的 demo_app.exe 杀掉。
add_test(NAME test_funq_app
    COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_funq_tests.py")

# 超时按"每条用例都要重新拉起一次被测程序"算：注入本身有固定的 1 秒 sleep
# （runner_win.py），加上应用自身启动与登录，每条底价约 3 秒；本目录的用例
# 都是读界面属性的短用例（没有跑业务的那种），五条实测约 15 秒，300 秒留足了
# 余量。
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

# 功能区那几条用例只在 demo_app 真的有功能区时有意义：qss 路线由 BUILD_RIBBON
# 决定，fluent 路线（src/app 发布的 APP_DEMO_UI_ROUTE）一定没有。用环境变量而
# 不是 genex：set_tests_properties 的 ENVIRONMENT 对生成器表达式的支持随 CMake
# 版本而变，一个 if() 更省事也更清楚
get_property(app_demo_route GLOBAL PROPERTY APP_DEMO_UI_ROUTE)
if(BUILD_RIBBON AND NOT app_demo_route STREQUAL "fluent")
    list(APPEND FUNQ_TEST_ENV "FUNQ_RIBBON=1")
endif()

set_tests_properties(test_funq_app PROPERTIES
    TIMEOUT 300
    LABELS "APP;FUNQ"
    ENVIRONMENT "${FUNQ_TEST_ENV}")
