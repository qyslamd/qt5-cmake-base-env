#include "markdown_view.h"

#include <gtest/gtest.h>

#include <QHash>
#include <QString>
#include <QUrl>

// "复制按钮"链路里属于本模块的那一段：copy: 链接 → 要复制的原文。
//
// 为什么只钉这一段：物理点击没法用 funq 驱动（代码块是 qlitehtml 自绘的，
// 合成鼠标事件到不了它的链接命中判定，实测普通链接也点不动），"点击 →
// linkClicked 信号"是 qlitehtml 的既有能力（内链跳转一直在用）；信号之后
// 的拦截与查表是本模块写的，所以在这一层钉死。控件侧的接线（把回调挂上、
// 每次换文档重建表）由人工点一次确认。
namespace {

const QHash<QString, QString> kBlocks = {
    {QStringLiteral("0"), QStringLiteral("int a = 1;\n")},
    {QStringLiteral("1"), QStringLiteral("print(2)\n")},
};

TEST(MarkdownViewCopyTest, ResolvesRegisteredIdToRawSource) {
  EXPECT_EQ(CopyTextForLink(QUrl(QStringLiteral("copy:0")), kBlocks),
            QStringLiteral("int a = 1;\n"));
  EXPECT_EQ(CopyTextForLink(QUrl(QStringLiteral("copy:1")), kBlocks),
            QStringLiteral("print(2)\n"));
}

// 查不到的 id（旧文档的链接、手改过的 HTML）返回空串：调用方据此不动剪贴板，
// 宁可没反应，也不能把用户剪贴板里原来的东西清掉
TEST(MarkdownViewCopyTest, UnknownIdYieldsEmpty) {
  EXPECT_TRUE(
      CopyTextForLink(QUrl(QStringLiteral("copy:99")), kBlocks).isEmpty());
}

// 非 copy: 的链接不是复制按钮，一律返回空串（由调用方转发给宿主）
TEST(MarkdownViewCopyTest, OtherSchemesYieldEmpty) {
  EXPECT_TRUE(
      CopyTextForLink(QUrl(QStringLiteral("https://example.com/")), kBlocks)
          .isEmpty());
  EXPECT_TRUE(CopyTextForLink(QUrl(QStringLiteral("file:///tmp/a.md")), kBlocks)
                  .isEmpty());
}

}  // namespace
