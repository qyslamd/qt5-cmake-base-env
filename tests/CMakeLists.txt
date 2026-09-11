# ============== Qt 测试运行环境变量 ==============
set(TEST_ENV_QT
    "PATH=${QT_BIN_DIR};$ENV{PATH}"
    "QT_PLUGIN_PATH=${QT_PLUGINS_DIR}"
)

# ============== 全局通用测试配置 ==============
# 公共头文件路径，所有测试通用
set(TEST_COMMON_INCLUDES
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googletest/include
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googlemock/include
)
# 公共链接库，所有测试通用
set(TEST_COMMON_LIBS gtest gtest_main gmock)
# 测试产物专属输出目录，与生产代码隔离
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests)
# 默认测试超时 10 秒，死循环自动杀进程
set(TEST_DEFAULT_TIMEOUT 10 CACHE STRING "Default test timeout in seconds")

# ============== 通用测试构建宏 ==============
# 参数说明：
#   NAME [必填]: 测试目标名
#   SOURCES [必填]: 源文件列表
#   LIBS [选填]: 额外链接的库（如被测模块）
#   INCLUDES [选填]: 额外头文件路径
#   QT_MODULES [选填]: 依赖的 Qt 模块（如 Core Widgets，自动加 Qt5:: 前缀）
#   LABELS [选填]: 测试分组标签，方便过滤
#   NO_RUN_SCRIPT [选填]: 不生成 Windows 的 run_test.bat
#   NO_GTEST_MAIN [选填]: 不链接 gtest_main（测试自提供 main，
#                         如集成测试需建 QCoreApplication）
macro(add_TEST_test)
    cmake_parse_arguments(ETEST "NO_RUN_SCRIPT;NO_GTEST_MAIN" "NAME"
        "SOURCES;LIBS;INCLUDES;QT_MODULES;LABELS" ${ARGN})

    # 1. 创建测试目标
    add_executable(${ETEST_NAME} ${ETEST_SOURCES})

    # 2. 统一 C++ 标准，与生产代码对齐
    set_target_properties(${ETEST_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS ON
    )

    # 3. Qt 依赖自动配置：传了 QT_MODULES 才开 AUTOMOC
    if(ETEST_QT_MODULES)
        set_target_properties(${ETEST_NAME} PROPERTIES AUTOMOC ON)
        list(TRANSFORM ETEST_QT_MODULES PREPEND "Qt5::")
        list(APPEND ETEST_LIBS ${ETEST_QT_MODULES})
    endif()

    # 4. 统一头文件路径
    target_include_directories(${ETEST_NAME} PRIVATE
        ${TEST_COMMON_INCLUDES} ${ETEST_INCLUDES})

    # 5. 统一链接库：公共库 + 用户指定库
    #    NO_GTEST_MAIN 时不链 gtest_main（测试提供自定义 main）
    if(ETEST_NO_GTEST_MAIN)
        set(_TEST_GTEST_LIBS gtest gmock)
    else()
        set(_TEST_GTEST_LIBS gtest gtest_main gmock)
    endif()
    target_link_libraries(${ETEST_NAME} PRIVATE
        ${_TEST_GTEST_LIBS} ${ETEST_LIBS})

    # 6. MSVC 编译选项，与生产代码一致
    if(MSVC)
        target_compile_options(${ETEST_NAME} PRIVATE /utf-8)
        target_compile_definitions(${ETEST_NAME} PRIVATE
            UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
    endif()

    # 7. 注册到 CTest
    add_test(NAME ${ETEST_NAME} COMMAND ${ETEST_NAME}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

    # 8. 测试属性：超时 + 分组 + Qt 运行环境
    set_tests_properties(${ETEST_NAME} PROPERTIES
        TIMEOUT ${TEST_DEFAULT_TIMEOUT}
        LABELS "${ETEST_LABELS}"
        ENVIRONMENT "${TEST_ENV_QT}"
    )

    # 9. Windows 下生成 run_test.bat
    if(WIN32 AND NOT ETEST_NO_RUN_SCRIPT)
        set(TARGET_NAME ${ETEST_NAME})
        configure_file("${CMAKE_SOURCE_DIR}/cmake/run_app.bat.in"
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/run_${ETEST_NAME}.bat" @ONLY)
    endif()
endmacro()

# ============== 子目录注册 ==============

