# funq 的「手动分离运行」示例。
#
# 与 tests/gallery_funq 的分工：
#   那边  一条命令跑完的回归测试，被测程序由用例自己拉起（用 gallery 当靶子，
#         因为它控件多、无登录，适合压 funq 的集成）
#   这边  两个终端各起一边的手动示例，便于看清 funq 的分工。靶子是本仓自己的
#         demo_app——「怎么测我自己的程序」才是这个示例要回答的问题
#
# 两份启动器由 configure_file 生成到构建目录：里面的路径都是本机的（Qt 的
# bin、funq 的两个包、demo_app 的位置），入库没有意义。

if(NOT BUILD_FUNQ)
    message(STATUS "BUILD_FUNQ=OFF，跳过 funq 手动示例")
    return()
endif()

find_program(PYTHON_EXECUTABLE NAMES python python3)
if(NOT PYTHON_EXECUTABLE)
    message(WARNING
        "未找到 Python，funq 手动示例不会生成。"
        "funq 需要 Python 3.8+（仅测试需要，编译不需要）")
    return()
endif()

if(NOT TARGET ${APP_TARGET_PREFIX}_app)
    message(STATUS "未构建 ${APP_TARGET_PREFIX}_app，跳过 funq 手动示例")
    return()
endif()

set(FUNQ_CLIENT_DIR "${CMAKE_SOURCE_DIR}/3rdparty/funq-master/client")
set(FUNQ_SERVER_DIR "${CMAKE_SOURCE_DIR}/3rdparty/funq-master/server")
set(DEMO_CLIENT_PY "${CMAKE_CURRENT_SOURCE_DIR}/funq_client_demo.py")

# 被测程序的产物路径。这里不能用 $<TARGET_FILE:...>——configure_file 在配置
# 阶段展开，生成器表达式要到生成阶段才有值。顶层已把运行时输出统一到
# <构建目录>/bin，按那个约定拼即可
set(DEMO_APP_EXE
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${APP_TARGET_PREFIX}_app${CMAKE_EXECUTABLE_SUFFIX}")

# server 侧与 client 侧共用这一个端口；换了它两边一起换
set(FUNQ_DEMO_PORT 9000 CACHE STRING "funq 手动示例里 libFunq 监听的端口")

if(WIN32)
    set(DEMO_SCRIPT_SUFFIX ".bat")
    configure_file(serve_demo_app.bat.in
        "${CMAKE_CURRENT_BINARY_DIR}/serve_demo_app${DEMO_SCRIPT_SUFFIX}" @ONLY)
    configure_file(run_demo_client.bat.in
        "${CMAKE_CURRENT_BINARY_DIR}/run_demo_client${DEMO_SCRIPT_SUFFIX}" @ONLY)
else()
    set(DEMO_SCRIPT_SUFFIX ".sh")
    configure_file(serve_demo_app.sh.in
        "${CMAKE_CURRENT_BINARY_DIR}/serve_demo_app${DEMO_SCRIPT_SUFFIX}" @ONLY)
    configure_file(run_demo_client.sh.in
        "${CMAKE_CURRENT_BINARY_DIR}/run_demo_client${DEMO_SCRIPT_SUFFIX}" @ONLY)
    file(CHMOD "${CMAKE_CURRENT_BINARY_DIR}/serve_demo_app${DEMO_SCRIPT_SUFFIX}"
               "${CMAKE_CURRENT_BINARY_DIR}/run_demo_client${DEMO_SCRIPT_SUFFIX}"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE)
endif()

message(STATUS
    "funq 手动示例：先跑 ${CMAKE_CURRENT_BINARY_DIR}/serve_demo_app${DEMO_SCRIPT_SUFFIX}，"
    "再跑同目录的 run_demo_client${DEMO_SCRIPT_SUFFIX}")
