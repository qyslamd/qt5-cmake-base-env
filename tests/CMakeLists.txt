# 测试公共配置与 add_unit_test 宏定义在 cmake/test_helpers.cmake，
# 由本文件引入后各测试子目录直接使用
include(${CMAKE_SOURCE_DIR}/cmake/test_helpers.cmake)

# ============== 子目录注册 ==============

add_subdirectory(core)
