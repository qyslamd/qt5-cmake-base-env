#include <gtest/gtest.h>

#include <QApplication>
#include <QMetaObject>
#include <QWidget>

#include "markdown_view.h"

// 开关的行为判据。用真正的 MarkdownView + QLiteHtmlWidget（offscreen），
// 通过 Q_INVOKABLE html() 读回渲染后的 HTML；不依赖控件内部类型。
//
// 执行顺序有意义：gtest 按定义顺序跑，"关闭时不加载引擎"的用例必须排在
// 任何会真正渲染公式/Mermaid 的用例之前（本进程的引擎是共享的）。
int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  // LoadDefaultCss 只在首次 ApplyDocument 读该变量；外部残留会让主题断言
  // 看错 CSS
  qunsetenv("MD_MARKDOWN_CSS");
  QApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

namespace {

QString HtmlOf(MarkdownView& view) {
  QWidget* html = view.findChild<QWidget*>(QStringLiteral("markdownHtml"));
  if (html == nullptr) {
    return {};
  }
  QString text;
  QMetaObject::invokeMethod(html, "html", Q_RETURN_ARG(QString, text));
  return text;
}

TEST(RenderTogglesTest, DisabledRenderersDoNotLoadOrRender) {
  MarkdownView view;
  view.SetFormulaRenderingEnabled(false);
  view.SetMermaidRenderingEnabled(false);
  view.SetContent(
      QStringLiteral("价格 $x^2$ 收尾\n\n"
                     "```mermaid\nflowchart LR\n    A --> B\n```\n"),
      QString());

  const QString html = HtmlOf(view);
  ASSERT_FALSE(html.isEmpty());
  EXPECT_TRUE(html.contains(QStringLiteral("$x^2$"))) << html.toStdString();
  EXPECT_FALSE(html.contains(QStringLiteral("data:image/png;base64")))
      << html.toStdString();
  // mermaid 围栏退回普通代码块
  EXPECT_TRUE(html.contains(QStringLiteral("<pre><code")))
      << html.toStdString();

  const MarkdownRendererStatus status = view.renderer_status();
  EXPECT_FALSE(status.formula.loaded);
  EXPECT_FALSE(status.mermaid.loaded);
}

TEST(RenderTogglesTest, EnablingFormulaRendersOnReapply) {
  MarkdownView view;
  view.SetFormulaRenderingEnabled(false);
  view.SetContent(QStringLiteral("$x^2$\n"), QString());
  EXPECT_FALSE(view.renderer_status().formula.loaded);

  view.SetFormulaRenderingEnabled(true);
  const QString html = HtmlOf(view);
  EXPECT_TRUE(html.contains(QStringLiteral("data:image/png;base64")))
      << html.toStdString();
  EXPECT_TRUE(view.renderer_status().formula.loaded);
}

TEST(RenderTogglesTest, LineNumbersToggleChangesStructure) {
  MarkdownView view;
  view.SetContent(QStringLiteral("```cpp\nint a;\n```\n"), QString());
  ASSERT_TRUE(HtmlOf(view).contains(QStringLiteral("<pre><code")));

  view.SetCodeLineNumbers(true);
  QString html = HtmlOf(view);
  EXPECT_TRUE(html.contains(QStringLiteral("<table class=\"md-code\">")))
      << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("md-code-ln")))
      << html.toStdString();

  view.SetCodeLineNumbers(false);
  html = HtmlOf(view);
  EXPECT_TRUE(html.contains(QStringLiteral("<pre><code")))
      << html.toStdString();
  // 注意不能断言不含 "md-code"：整份文档里带着 CSS 规则
  EXPECT_FALSE(html.contains(QStringLiteral("<table class=\"md-code\">")))
      << html.toStdString();
}

TEST(RenderTogglesTest, StatusCountsRenders) {
  MarkdownView view;
  view.ResetRendererStats();
  view.SetContent(QStringLiteral("```cpp\nint a;\n```\n"), QString());
  const MarkdownRendererStatus status = view.renderer_status();
  EXPECT_EQ(status.highlight_context.renders, 1U);
}

// 主题切换 = 换样式表 + 重渲染（公式颜色变了，缓存 key 也变）
TEST(RenderTogglesTest, ThemeSwitchUpdatesCssAndRerenders) {
  MarkdownView view;
  view.SetContent(QStringLiteral("$x$\n"), QString());
  EXPECT_EQ(view.theme(), MarkdownTheme::Light);
  const QString light_html = HtmlOf(view);
  ASSERT_FALSE(light_html.isEmpty());
  const quint64 renders = view.renderer_status().formula_context.renders;
  ASSERT_GE(renders, 1U);

  view.SetTheme(MarkdownTheme::Dark);
  EXPECT_EQ(view.theme(), MarkdownTheme::Dark);
  const QString dark_html = HtmlOf(view);
  EXPECT_NE(light_html, dark_html);
  EXPECT_TRUE(dark_html.contains(QStringLiteral("#0d1117")))
      << dark_html.toStdString();
  // 新颜色是新的缓存 key，必须真的再渲染一次
  EXPECT_EQ(view.renderer_status().formula_context.renders, renders + 1);
}

}  // namespace
