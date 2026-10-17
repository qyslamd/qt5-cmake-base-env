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

# 标题锚点：slug 规则与 ASCII id 的生成是纯字符串变换，可逐条断言。
# 为什么 id 非 ASCII 不可、data-md-slug 为什么要跟着走，见用例头部注释
add_unit_test(
    NAME test_heading_ids
    SOURCES heading_ids_test.cpp
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

# mermaid 的两层后处理是纯字符串→字符串，毫秒级，可逐条断言
add_unit_test(
    NAME test_mermaid_svg_cleanup
    SOURCES mermaid_svg_cleanup_test.cpp
    LIBS ${APP_TARGET_PREFIX}_markdown
    QT_MODULES Core
    LABELS MARKDOWN
)

# mermaid 端到端：首次要 eval 3.5MB 的 bundle（秒级），默认 10 秒超时会被 ctest
# 直接杀掉，所以单独放宽——add_unit_test 没有 TIMEOUT 参数，只能在这里补
add_unit_test(
    NAME test_mermaid_renderer
    SOURCES mermaid_renderer_test.cpp
    LIBS ${APP_TARGET_PREFIX}_markdown
    QT_MODULES Gui
    LABELS MARKDOWN
    NO_GTEST_MAIN
)
set_tests_properties(test_mermaid_renderer PROPERTIES TIMEOUT 120)

# "复制按钮"链路里属于本模块的一段：copy: 链接 → 原文（纯函数）。
# 物理点击 funq 驱动不了（代码块是 qlitehtml 自绘的），见用例头部注释
add_unit_test(
    NAME test_markdown_render_context
    SOURCES render_context_test.cpp
    LIBS ${APP_TARGET_PREFIX}_markdown
    QT_MODULES Core
    LABELS MARKDOWN
)

# 开关：公式/Mermaid 关闭时不加载引擎，行号切换改结构。要 Widgets 与
# QApplication；NO_GTEST_MAIN 是因为要自己建 application
add_unit_test(
    NAME test_render_diagnostics
    SOURCES render_diagnostics_test.cpp
    LIBS ${APP_TARGET_PREFIX}_markdown
    QT_MODULES Core
    LABELS MARKDOWN
)

# 主题 → 样式表/公式色的映射：qrc 别名与代码路径分居两处，需要一条断言
# 把它们钉在一起
add_unit_test(
    NAME test_markdown_theme
    SOURCES theme_test.cpp
    LIBS ${APP_TARGET_PREFIX}_markdown
    QT_MODULES Core
    LABELS MARKDOWN
)

# MD_MARKDOWN_CSS 覆盖的语义：CSS 优先、主题只切公式/Mermaid、非 Light
# 主题切换要提示。必须独立进程——环境变量只在首次 LoadDefaultCss 时读
add_unit_test(
    NAME test_css_override
    SOURCES css_override_test.cpp
    LIBS ${APP_TARGET_PREFIX}_markdown
    QT_MODULES Widgets
    LABELS MARKDOWN
    NO_GTEST_MAIN
)

# 持久缓存：key 组成、命中/损坏/关闭/淘汰/清理。全部用临时目录
add_unit_test(
    NAME test_render_cache
    SOURCES render_cache_test.cpp
    LIBS ${APP_TARGET_PREFIX}_markdown
    QT_MODULES Core
    LABELS MARKDOWN
)

# 大纲控件：层级结构、点击回跳、当前项高亮。QTest 用于点击命中
add_unit_test(
    NAME test_outline_widget
    SOURCES outline_widget_test.cpp
    LIBS ${APP_TARGET_PREFIX}_markdown
    QT_MODULES Widgets Test
    LABELS MARKDOWN
    NO_GTEST_MAIN
)

add_unit_test(
    NAME test_render_toggles
    SOURCES render_toggles_test.cpp
    LIBS ${APP_TARGET_PREFIX}_markdown
    QT_MODULES Widgets
    LABELS MARKDOWN
    NO_GTEST_MAIN
)

add_unit_test(
    NAME test_markdown_view
    SOURCES markdown_view_test.cpp
    LIBS ${APP_TARGET_PREFIX}_markdown
    QT_MODULES Core
    LABELS MARKDOWN
)

# 查找与打印：渲染控件的既有能力，模块只做透传。判据取行为最小面——找得到/
# 找不到、PDF 真的出得来。无头（offscreen + QPrinter(PdfFormat) 写临时文件），
# 要 Widgets 与 PrintSupport；NO_GTEST_MAIN 是因为要自己建 QApplication
add_unit_test(
    NAME test_markdown_view_actions
    SOURCES markdown_view_actions_test.cpp
    LIBS ${APP_TARGET_PREFIX}_markdown
    QT_MODULES Widgets PrintSupport
    LABELS MARKDOWN
    NO_GTEST_MAIN
)
