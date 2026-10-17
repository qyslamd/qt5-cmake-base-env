# qjsc 冒烟测试：qjs-libc 在 MSVC 2019 下要靠 0003 补丁的原子操作 shim 才编得
# 过；测试目标依赖 qjsc，构建测试时就会把这条 EXCLUDE_FROM_ALL 目标带出来，
# 补丁失效会先在这里炸，而不是等到有人手工 --target qjsc 才发现
add_unit_test(
    NAME test_qjsc
    SOURCES qjsc_test.cpp
    QT_MODULES Core
    LABELS QJSC
)
add_dependencies(test_qjsc qjsc)
target_compile_definitions(test_qjsc PRIVATE
    QJSC_PATH="$<TARGET_FILE:qjsc>")
