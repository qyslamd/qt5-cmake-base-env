#include "code_highlighter.h"

#include <gtest/gtest.h>

#include <QString>

namespace {

TEST(CodeHighlighterTest, ColorsKeywordsStringsNumbersAndComments) {
  const QString html = HighlightCode(
      QStringLiteral(
          "class A { int n = 42; const char* s = \"x\"; };  // 说明"),
      QStringLiteral("cpp"));
  EXPECT_TRUE(html.contains(QStringLiteral("<span class=\"kw\">class</span>")))
      << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("<span class=\"num\">42</span>")))
      << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("<span class=\"str\">\"x\"</span>")))
      << html.toStdString();
  EXPECT_TRUE(
      html.contains(QStringLiteral("<span class=\"cmt\">// 说明</span>")))
      << html.toStdString();
}

// 块注释要跨行保持状态：第二行若被当成普通代码，配色会在注释中间断掉。
// 注释符单独一段（进入注释那一步就吐出来了），所以逐行断言而不是整段
TEST(CodeHighlighterTest, BlockCommentSpansLines) {
  const QString html = HighlightCode(
      QStringLiteral("/* 第一行\n第二行 */ int a;"), QStringLiteral("cpp"));
  EXPECT_TRUE(
      html.contains(QStringLiteral("<span class=\"cmt\"> 第一行</span>")))
      << html.toStdString();
  EXPECT_TRUE(
      html.contains(QStringLiteral("<span class=\"cmt\">第二行 */</span>")))
      << html.toStdString();
  // 注释结束后代码恢复着色
  EXPECT_TRUE(html.contains(QStringLiteral("<span class=\"kw\">int</span>")))
      << html.toStdString();
  EXPECT_FALSE(html.contains(QStringLiteral("cmt\"> int")))
      << html.toStdString();
}

TEST(CodeHighlighterTest, HashCommentsForShellAndCmake) {
  const QString shell =
      HighlightCode(QStringLiteral("echo 1  # 说明"), QStringLiteral("bash"));
  EXPECT_TRUE(
      shell.contains(QStringLiteral("<span class=\"cmt\"># 说明</span>")))
      << shell.toStdString();
  EXPECT_TRUE(shell.contains(QStringLiteral("<span class=\"kw\">echo</span>")))
      << shell.toStdString();

  const QString cmake =
      HighlightCode(QStringLiteral("if(X)  # 条件"), QStringLiteral("cmake"));
  EXPECT_TRUE(cmake.contains(QStringLiteral("<span class=\"kw\">if</span>")))
      << cmake.toStdString();
}

// 每种语言的注释符不能串味：JSON 没有注释，CSS/QSS 只有块注释
TEST(CodeHighlighterTest, CommentSyntaxDoesNotLeakAcrossLanguages) {
  const QString json = HighlightCode(QStringLiteral("{\"a\": true}  # x"),
                                     QStringLiteral("json"));
  EXPECT_FALSE(json.contains(QStringLiteral("class=\"cmt\"")))
      << json.toStdString();
  EXPECT_TRUE(json.contains(QStringLiteral("<span class=\"kw\">true</span>")))
      << json.toStdString();

  const QString qss = HighlightCode(QStringLiteral("a { color: red; } // x"),
                                    QStringLiteral("qss"));
  EXPECT_FALSE(qss.contains(QStringLiteral("class=\"cmt\"")))
      << qss.toStdString();
}

TEST(CodeHighlighterTest, UnknownLanguageFallsBackToCpp) {
  const QString html = HighlightCode(QStringLiteral("int a; // 注释"),
                                     QStringLiteral("brainfuck"));
  EXPECT_TRUE(html.contains(QStringLiteral("<span class=\"kw\">int</span>")))
      << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("class=\"cmt\"")))
      << html.toStdString();
}

// 高亮结果直接拼进 HTML，源码里的尖括号与 & 必须先转义，否则会变成真标签
TEST(CodeHighlighterTest, EscapesHtmlSpecials) {
  const QString html = HighlightCode(QStringLiteral("if (a < b && c > d) {}"),
                                     QStringLiteral("cpp"));
  EXPECT_FALSE(html.contains(QStringLiteral("< b"))) << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("&lt;"))) << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("&amp;&amp;")))
      << html.toStdString();
  EXPECT_TRUE(html.contains(QStringLiteral("&gt;"))) << html.toStdString();
}

TEST(CodeHighlighterTest, EmptyInputStaysEmpty) {
  EXPECT_TRUE(HighlightCode(QString(), QStringLiteral("cpp")).isEmpty());
}

}  // namespace
