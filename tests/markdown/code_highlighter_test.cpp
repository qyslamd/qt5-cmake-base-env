#include "code_highlighter.h"

#include <gtest/gtest.h>

#include <QString>
#include <QStringList>

// 高亮的端到端判据：qrc 载荷 → QuickJS 里跑 highlight.js → 类名映射后的
// HTML。类名（kw/str/num/cmt/…）是与 markdown_default.css 的唯一契约，这里
// 钉的是"关键字/字符串有没有被认出来""每行的 span 是否配平"这类静默出错的
// 地方；配色本身是 CSS 的事
namespace {

TEST(CodeHighlighterTest, ColorsCppKeywordsStringsNumbersAndComments) {
  const QString html = HighlightCode(
      QStringLiteral(
          "class A { int n = 42; const char* s = \"x\"; };  // 说明"),
      QStringLiteral("cpp"));
  EXPECT_TRUE(html.contains(QStringLiteral("class=\"kw\"")))
      << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("class=\"num\"")))
      << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("class=\"str\"")))
      << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("class=\"cmt\"")))
      << html.toStdString();
  // 内容还在（转义过的）
  EXPECT_TRUE(html.contains(QStringLiteral("42"))) << html.toStdString();
}

// 围栏标签到 hljs 语言的别名（py→python、bat→dos…）要在入口里处理
TEST(CodeHighlighterTest, LanguageAliasesResolve) {
  const QString py = HighlightCode(QStringLiteral("def f():\n    pass\n"),
                                   QStringLiteral("py"));
  EXPECT_TRUE(py.contains(QStringLiteral("class=\"kw\""))) << py.toStdString();

  const QString bat =
      HighlightCode(QStringLiteral("echo hello\n"), QStringLiteral("bat"));
  EXPECT_FALSE(bat.isEmpty()) << bat.toStdString();
}

// 未注册的语言不猜（demo 版会 highlightAuto，这里有意关掉：猜错的配色比不
// 高亮更难看）——返回纯转义的原文，一个 span 都没有
TEST(CodeHighlighterTest, UnknownLanguageFallsBackToPlainEscape) {
  const QString html =
      HighlightCode(QStringLiteral("a < b & c"), QStringLiteral("notalang"));
  EXPECT_EQ(html, QStringLiteral("a &lt; b &amp; c")) << html.toStdString();
  EXPECT_FALSE(html.contains(QStringLiteral("<span"))) << html.toStdString();
}

// 空输入返回空串（markdown_to_html 对空代码块本来就不该走到这里）
TEST(CodeHighlighterTest, EmptyInputYieldsEmpty) {
  EXPECT_EQ(HighlightCode(QString(), QStringLiteral("cpp")), QString());
}

// 跨行 span 拆行：C++ 侧按 \n 处理代码块时，任何一行的 <span 与 </span>
// 数量都必须相等，否则 HTML 标签配对会断（splitSpansPerLine 的职责）
TEST(CodeHighlighterTest, SpansAreBalancedOnEveryLine) {
  const QString html = HighlightCode(
      QStringLiteral("/* 第一行\n第二行 */ int a = 1;\n// done\n"),
      QStringLiteral("cpp"));
  const QStringList lines = html.split(QChar('\n'));
  ASSERT_GT(lines.size(), 1);
  for (const QString& line : lines) {
    const int open = line.count(QStringLiteral("<span"));
    const int close = line.count(QStringLiteral("</span>"));
    EXPECT_EQ(open, close) << line.toStdString();
  }
}

// 输出里的类名必须都是 CSS 认识的短类名（hljs-xxx 前缀如果漏映射，样式就
// 静默丢失——配色看着"对了一半"最难查）
TEST(CodeHighlighterTest, NoUnmappedHljsClassNamesRemain) {
  const QString html = HighlightCode(
      QStringLiteral("#include <vector>\nstruct S { double x; };\n"),
      QStringLiteral("cpp"));
  EXPECT_FALSE(html.contains(QStringLiteral("hljs-"))) << html.toStdString();
}

}  // namespace
