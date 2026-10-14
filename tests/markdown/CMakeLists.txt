# 文档预览的渲染判据。
#
# 这两条都是纯函数（Markdown → HTML、源码 → 着色片段），毫秒级、可逐分支断言，
# 不必拉起窗口：渲染出来长什么样是像素的事（见 scripts/funq/probe_markdown_view.py），
# 这里钉的是"表格有没有被丢掉""代码里的 < 有没有逃出去"这类会静默出错的地方
add_unit_test(
    NAME test_markdown_to_html
    SOURCES markdown_to_html_test.cpp
    LIBS ${APP_TARGET_PREFIX}_markdown
    QT_MODULES Core
    LABELS MARKDOWN
)

add_unit_test(
    NAME test_code_highlighter
    SOURCES code_highlighter_test.cpp
    LIBS ${APP_TARGET_PREFIX}_markdown
    QT_MODULES Core
    LABELS MARKDOWN
)
