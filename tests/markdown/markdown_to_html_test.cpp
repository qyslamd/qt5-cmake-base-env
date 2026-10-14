#include "markdown_to_html.h"

#include <gtest/gtest.h>

#include <QString>

namespace {

// GFM 的表格是这套渲染的硬需求之一：用默认 flags 解析时 md4c 会静默丢掉它，
// 渲染结果里只剩一堆竖线，所以这条必须钉住
TEST(MarkdownToHtmlTest, RendersGfmTable) {
  const QString html =
      MarkdownToHtml(QStringLiteral("| 列 A | 列 B |\n|---|---|\n| 1 | 2 |\n"));
  EXPECT_TRUE(html.contains(QStringLiteral("<table>"))) << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("<th"))) << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("<td"))) << html.toStdString();
}

TEST(MarkdownToHtmlTest, RendersHeadingsAndInlineCode) {
  const QString html =
      MarkdownToHtml(QStringLiteral("# 标题\n\n正文 `inline` 结束。\n"));
  EXPECT_TRUE(html.contains(QStringLiteral("<h1>标题</h1>")))
      << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("<code>inline</code>")))
      << html.toStdString();
}

// 围栏代码块要留住语言标记（样式与调试都靠它），代码本身要过一遍高亮器
TEST(MarkdownToHtmlTest, HighlightsFencedCodeBlock) {
  const QString html =
      MarkdownToHtml(QStringLiteral("```cpp\nclass Widget {};\n```\n"));
  EXPECT_TRUE(html.contains(QStringLiteral("class=\"language-cpp\"")))
      << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("<span class=\"kw\">class</span>")))
      << html.toStdString();
}

TEST(MarkdownToHtmlTest, CodeBlockWithoutLanguageHasNoClass) {
  const QString html = MarkdownToHtml(QStringLiteral("```\nplain\n```\n"));
  EXPECT_TRUE(html.contains(QStringLiteral("<pre><code>")))
      << html.toStdString();
}

// 回归点：代码块里的 "<" 从 md4c 出来是 &lt;，高亮器还原再转义，一进一出
// 之后必须还是 &lt;——漏了任何一半都会让代码里的尖括号变成真标签
TEST(MarkdownToHtmlTest, CodeBlockKeepsAngleBracketsEscaped) {
  const QString html =
      MarkdownToHtml(QStringLiteral("```cpp\nif (a < b && c > d) {}\n```\n"));
  EXPECT_TRUE(html.contains(QStringLiteral("&lt;"))) << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("&amp;&amp;")))
      << html.toStdString();
  EXPECT_FALSE(html.contains(QStringLiteral("a < b"))) << html.toStdString();
}

// 原始 HTML 按 GitHub 的惯例放行（文档里常用 <br>、<sub>），qlitehtml 不执行
// 脚本；但裸的 "<" 仍要被转义，别让它把后面的文字吞成标签
TEST(MarkdownToHtmlTest, KeepsRawHtmlButEscapesStrayAngle) {
  const QString html = MarkdownToHtml(QStringLiteral("上行<br>下行 a < b\n"));
  EXPECT_TRUE(html.contains(QStringLiteral("<br>"))) << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("a &lt; b"))) << html.toStdString();
}

// 回归点：md4c 在 GFM 下放行内联原始 HTML，正文里没闭合的裸 <pre><code> 若不加
// 行首锚点，会一直匹配到文档后面下一个 </code></pre>，把中间整段正文吞成代码
TEST(MarkdownToHtmlTest, StrayInlinePreCodeDoesNotSwallowLaterBlocks) {
  const QString html = MarkdownToHtml(
      QStringLiteral("text <pre><code> tail\n\n```cpp\nint a;\n```\n"));
  // 后面那个真围栏块照常成块且带上语言类
  EXPECT_TRUE(html.contains(QStringLiteral("class=\"language-cpp\"")))
      << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("<span class=\"kw\">int</span>")))
      << html.toStdString();
  // 正文那截没被当成代码吞掉
  EXPECT_FALSE(html.contains(QStringLiteral("tail&lt;/p&gt;")))
      << html.toStdString();
}

// 代码块里的实体往返：md4c 转义一次、高亮器还原再转义一次，字面量不能被二次解码
TEST(MarkdownToHtmlTest, CodeBlockEntitiesRoundTrip) {
  const QString html =
      MarkdownToHtml(QStringLiteral("```\na &amp;lt; b\nA & B\n```\n"));
  EXPECT_TRUE(html.contains(QStringLiteral("&amp;amp;lt;")))
      << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("A &amp; B"))) << html.toStdString();
}

// 未闭合围栏由 md4c 自己补齐；空输入不该产出半截标签
TEST(MarkdownToHtmlTest, UnclosedFenceAndEmptyInput) {
  const QString html = MarkdownToHtml(QStringLiteral("```cpp\nint a;\n"));
  EXPECT_TRUE(html.contains(QStringLiteral("</code></pre>")))
      << html.toStdString();

  const QString empty = MarkdownToHtml(QString());
  EXPECT_FALSE(empty.contains(QStringLiteral("<pre>"))) << empty.toStdString();
}

}  // namespace
