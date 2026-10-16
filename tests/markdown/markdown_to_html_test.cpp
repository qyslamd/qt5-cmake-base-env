#include "markdown_to_html.h"

#include <gtest/gtest.h>

#include <QString>
#include <QStringList>
#include <QVector>

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
// 之后必须还是 &lt;——漏了任何一半都会让代码里的尖括号变成真标签。
// 引号：highlight.js 对字符串里的 ' 用数字实体 &#x27; 转义，显示等价，所以
// 钉的是它而不是字面 '（&apos; 那条还原规则照旧要认，字面引号不许漏成
// "&apos;" 的样子）
TEST(MarkdownToHtmlTest, CodeBlockKeepsAngleBracketsEscaped) {
  const QString html = MarkdownToHtml(
      QStringLiteral("```cpp\nif (a < b && c > d) { x = 'y'; }\n```\n"));
  EXPECT_TRUE(html.contains(QStringLiteral("&lt;"))) << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("&amp;&amp;")))
      << html.toStdString();
  EXPECT_FALSE(html.contains(QStringLiteral("a < b"))) << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("&#x27;y&#x27;")))
      << html.toStdString();
  EXPECT_FALSE(html.contains(QStringLiteral("apos;"))) << html.toStdString();
}

// 原始 HTML 按 GitHub 的惯例放行（文档里常用 <br>、<sub>），qlitehtml 不执行
// 脚本；但裸的 "<" 仍要被转义，别让它把后面的文字吞成标签
TEST(MarkdownToHtmlTest, KeepsRawHtmlButEscapesStrayAngle) {
  const QString html = MarkdownToHtml(QStringLiteral("上行<br>下行 a < b\n"));
  EXPECT_TRUE(html.contains(QStringLiteral("<br>"))) << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("a &lt; b"))) << html.toStdString();
}

// ---------- 公式（抽取与回填是纯逻辑，假渲染器就能验，不必起
// QuickJS）----------

// 记下回调收到的内容，同时返回一个能一眼认出的标记
struct FormulaProbe {
  QStringList texes;
  QVector<bool> displays;

  FormulaSink Sink() {
    return [this](const QString& tex, bool display) {
      texes << tex;
      displays << display;
      return QStringLiteral("<img data-tex=\"%1\">").arg(tex);
    };
  }
};

TEST(MarkdownToHtmlTest, FormulasGoThroughTheSink) {
  FormulaProbe probe;
  const QString html =
      MarkdownToHtml(QStringLiteral("质能 $E = mc^2$ 收尾\n"), probe.Sink());
  ASSERT_EQ(probe.texes.size(), 1);
  EXPECT_EQ(probe.texes.at(0), QStringLiteral("E = mc^2"));
  EXPECT_FALSE(probe.displays.at(0));
  EXPECT_TRUE(html.contains(QStringLiteral("<img data-tex=\"E = mc^2\">")))
      << html.toStdString();
}

// 单独成行的 $$ 会跨行合并成一段独立公式（tex 里的换行要去掉）
TEST(MarkdownToHtmlTest, DisplayBlockSpansLines) {
  FormulaProbe probe;
  const QString html = MarkdownToHtml(
      QStringLiteral("$$\n\\int_0^1 x\\,dx\n$$\n"), probe.Sink());
  ASSERT_EQ(probe.texes.size(), 1);
  EXPECT_TRUE(probe.displays.at(0));
  EXPECT_EQ(probe.texes.at(0).trimmed(), QStringLiteral("\\int_0^1 x\\,dx"));
  EXPECT_FALSE(html.contains(QStringLiteral("$$"))) << html.toStdString();
}

// 代码里的 $ 是字面量：围栏块、行内代码都不该被当成公式
TEST(MarkdownToHtmlTest, DollarsInCodeAreNotFormulas) {
  FormulaProbe probe;
  const QString html = MarkdownToHtml(
      QStringLiteral(
          "```bash\nawk '{ print $1 }'\n```\n\n行内 `$HOME` 与 $x$\n"),
      probe.Sink());
  ASSERT_EQ(probe.texes.size(), 1) << "只有末尾的 $x$ 该进回调";
  EXPECT_EQ(probe.texes.at(0), QStringLiteral("x"));
  EXPECT_TRUE(html.contains(QStringLiteral("print $1"))) << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("$HOME"))) << html.toStdString();
}

// 正文里的钱不该被配对；转义的 \$ 是字面量
TEST(MarkdownToHtmlTest, MoneyAndEscapedDollarStayText) {
  FormulaProbe probe;
  const QString html = MarkdownToHtml(
      QStringLiteral("价格在 $5 到 $10 之间，转义 \\$y\\$ 也是字面量\n"),
      probe.Sink());
  EXPECT_TRUE(probe.texes.isEmpty()) << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("$5 到 $10"))) << html.toStdString();
  EXPECT_FALSE(html.contains(QStringLiteral("data-tex"))) << html.toStdString();
}

// 渲染不出来（回调返回空）或调用方没给回调：原文照旧显示，别吞内容
TEST(MarkdownToHtmlTest, FailedFormulaFallsBackToSource) {
  const FormulaSink failing = [](const QString&, bool) { return QString(); };
  const QString html =
      MarkdownToHtml(QStringLiteral("看 $\\frac{a}{b}$ 这里\n"), failing);
  EXPECT_TRUE(html.contains(QStringLiteral("math-src"))) << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("\\frac{a}{b}")))
      << html.toStdString();

  const QString untouched = MarkdownToHtml(QStringLiteral("看 $x$ 这里\n"));
  EXPECT_TRUE(untouched.contains(QStringLiteral("$x$")))
      << untouched.toStdString();
}

// 没闭合的块级 $$：按原文吐回去，不能吃掉后面的内容
TEST(MarkdownToHtmlTest, UnclosedDisplayBlockStaysText) {
  FormulaProbe probe;
  const QString html = MarkdownToHtml(
      QStringLiteral("$$\n孤零零一段\n\n后面还有正文\n"), probe.Sink());
  EXPECT_TRUE(probe.texes.isEmpty()) << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("后面还有正文")))
      << html.toStdString();
}

// 回归点：md4c 在 GFM 下放行内联原始 HTML，正文里没闭合的裸 <pre><code> 若不加
// 行首锚点，会一直匹配到文档后面下一个 </code></pre>，把中间整段正文吞成代码
TEST(MarkdownToHtmlTest, StrayInlinePreCodeDoesNotSwallowLaterBlocks) {
  const QString html = MarkdownToHtml(
      QStringLiteral("text <pre><code> tail\n\n```cpp\nint a;\n```\n"));
  // 后面那个真围栏块照常成块且带上语言类；highlight.js 把裸类型归为
  // type（.ty）而不是 keyword——比旧手写分词器拆得细，这里顺带钉住
  EXPECT_TRUE(html.contains(QStringLiteral("class=\"language-cpp\"")))
      << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("<span class=\"ty\">int</span>")))
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
