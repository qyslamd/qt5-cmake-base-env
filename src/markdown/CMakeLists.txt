set(TARGET_NAME ${APP_TARGET_PREFIX}_markdown)

set(TARGET_SRCS
    code_highlighter.cpp
    code_highlighter.h
    formula_renderer.cpp
    formula_renderer.h
    heading_ids.cpp
    heading_ids.h
    mermaid_css_inliner.cpp
    mermaid_css_inliner.h
    mermaid_renderer.cpp
    mermaid_renderer.h
    mermaid_svg_cleanup.cpp
    mermaid_svg_cleanup.h
    markdown_to_html.cpp
    markdown_to_html.h
    markdown_view.cpp
    markdown_view.h
    markdown_window.cpp
    markdown_window.h
)

# 公式走 SVG→PNG（QSvgRenderer 在 Gui 里，Svg 模块提供 svg 支持）
find_package(Qt5 5.12 QUIET COMPONENTS Svg)
if(NOT Qt5Svg_FOUND)
    message(FATAL_ERROR
        "Qt5Svg 没找到：公式渲染依赖 QSvgRenderer（见 src/markdown/formula_renderer.cpp）")
endif()

set(MARKDOWN_RESOURCES markdown_res.qrc)
qt5_add_resources(MARKDOWN_RESOURCES_CPP ${MARKDOWN_RESOURCES})

add_library(${TARGET_NAME} STATIC ${TARGET_SRCS} ${MARKDOWN_RESOURCES_CPP})

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)

# PUBLIC：公开头用到 Qt 类型，宿主得连着拿到。
# qlitehtml / md4c 只在 .cpp 里出现，链接与头文件路径都不外传
target_link_libraries(${TARGET_NAME}
    PUBLIC
    Qt5::Widgets
    Qt5::Gui
    Qt5::Core
    PRIVATE
    qlitehtml
    md4c
    qjs            # 公式：在 QuickJS 里跑 MathJax（tex→svg）
    Qt5::Svg       # 公式：QSvgRenderer 把 SVG 画成位图
)

target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)

    # 线程栈放大：MathJax 在 QuickJS 里解析递归深，formula_renderer.cpp 里
    # 把 JS 配额放到 16MB，线程栈要更宽裕（JS 配额是软上限、线程栈是硬的，
    # 反过来会砸穿原生栈直接崩）。
    # 别贪大：32 位进程的地址空间只有 2GB，主线程栈 reserve 太大会让 funq 的
    # CreateRemoteThread 注入失败（实测 128MB 时报 WinError 8 内存资源不足）；
    # 32MB 在 x86/x64 上都实测可用。
    # 走 INTERFACE 是因为真正需要大栈的是"跑 JS 的那个进程"，即链接本模块的
    # 可执行文件；GCC 不需要（Linux 靠默认栈即可）
    target_link_options(${TARGET_NAME} INTERFACE "/STACK:33554432")
endif()
