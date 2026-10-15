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

# 公式渲染的端到端判据：载荷加载 → QuickJS 里 tex→svg → 画成 PNG → <img>。
# 这条链路以前只能靠 funq 抓像素看，而像素判据没法进 CI、也不好在无显示的
# 机器上跑；测试自己起 QGuiApplication 并切 offscreen 平台，无头也能验。
# 要 Gui 是因为 QSvgRenderer 走 QPainter；NO_GTEST_MAIN 是因为要自己建 application
add_unit_test(
    NAME test_formula_renderer
    SOURCES formula_renderer_test.cpp
    LIBS ${APP_TARGET_PREFIX}_markdown
    QT_MODULES Gui
    LABELS MARKDOWN
    NO_GTEST_MAIN
)
