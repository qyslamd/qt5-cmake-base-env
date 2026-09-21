# gallery 的 funq UI 测试。
#
# 与 add_unit_test 那套 gtest 用例不同：被测对象是另一个完整进程（gallery），
# 而且要真实窗口与事件循环，所以这里直接 add_test 拉起 python 脚本，由 funq
# 负责注入与驱动（见 run_funq_tests.py）。
#
# 三个前置缺一不可：gallery 可执行文件、funq 的库、以及 Python 解释器。
# 缺任何一个都只跳过并给出提示，不让 ctest 里出现一条注定失败的用例。

if(NOT BUILD_LIBUI_GALLERY)
    message(STATUS "BUILD_LIBUI_GALLERY=OFF，跳过 gallery 的 funq 测试")
    return()
endif()
if(NOT BUILD_FUNQ)
    message(STATUS "BUILD_FUNQ=OFF，跳过 gallery 的 funq 测试")
    return()
endif()

find_program(PYTHON_EXECUTABLE NAMES python python3)
if(NOT PYTHON_EXECUTABLE)
    message(WARNING
        "未找到 Python，gallery 的 funq 测试不会注册。"
        "funq 需要 Python 3.8+（仅测试需要，编译不需要）")
    return()
endif()

# gallery 的输出目录由 qt-libui 固定为 <build>/bin，名字即目标名
set(GALLERY_DIR "${CMAKE_BINARY_DIR}/bin")
set(GALLERY_EXE "${GALLERY_DIR}/libui_gallery${CMAKE_EXECUTABLE_SUFFIX}")
# 别名表入库（放源码树），改界面后用 probe_widgets.py 重新打印路径再更新它
set(GALLERY_ALIAS "${CMAKE_CURRENT_SOURCE_DIR}/gallery.alias")
if(NOT BUILD_LIBUI_GALLERY OR NOT TARGET libui_gallery)
    message(STATUS "未构建 libui_gallery 目标，跳过 gallery 的 funq 测试")
    return()
endif()

set(FUNQ_CLIENT_DIR "${CMAKE_SOURCE_DIR}/3rdparty/funq-master/client")
set(FUNQ_SERVER_DIR "${CMAKE_SOURCE_DIR}/3rdparty/funq-master/server")

configure_file(funq.conf.in "${CMAKE_CURRENT_BINARY_DIR}/funq.conf" @ONLY)
configure_file(funq_attach.bat.in
    "${CMAKE_CURRENT_BINARY_DIR}/funq_attach.bat" @ONLY)

add_test(NAME test_gallery_funq
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
set(GALLERY_TEST_ENV
    "FUNQ_CONF=${CMAKE_CURRENT_BINARY_DIR}/funq.conf"
    "FUNQ_ATTACH_EXE=${CMAKE_CURRENT_BINARY_DIR}/funq_attach.bat"
    "FUNQ_CLIENT_DIR=${FUNQ_CLIENT_DIR}"
    "FUNQ_SERVER_DIR=${FUNQ_SERVER_DIR}"
    "FUNQ_QT_BIN=${QT_BIN_DIR}")

set_tests_properties(test_gallery_funq PROPERTIES
    TIMEOUT 180
    LABELS "APP;FUNQ"
    ENVIRONMENT "${GALLERY_TEST_ENV}")
