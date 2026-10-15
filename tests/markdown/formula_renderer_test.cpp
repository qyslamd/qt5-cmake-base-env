#include "formula_renderer.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QGuiApplication>
#include <QString>

// 公式渲染的端到端判据：加载 qrc 里的 MathJax 载荷 → QuickJS 里 tex→svg →
// QSvgRenderer 画成 PNG → 内联成 <img>。整条链路要么全通要么全不通，中间任何
// 一环坏了都只能从"像素"看出来——所以在无头环境把产物本身断言掉。
//
// 要 QGuiApplication 而不是 QCoreApplication：QSvgRenderer 走 QPainter，
// 没有 GUI application 建不出绘图后端。平台用 offscreen，CI/无显示器也能跑。
int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
  QGuiApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

namespace {

TEST(FormulaRendererTest, RendersInlineFormulaToInlineImage) {
  ASSERT_TRUE(FormulaRendererReady()) << "qrc 里的 MathJax 载荷没加载起来";
  const QString tag = FormulaToImgTag(QStringLiteral("E = mc^2"), false);
  ASSERT_FALSE(tag.isEmpty()) << "渲染失败（会退回显示原文）";
  EXPECT_TRUE(tag.startsWith(QStringLiteral("<img"))) << tag.toStdString();
  EXPECT_TRUE(tag.contains(QStringLiteral("data:image/png;base64,")))
      << tag.toStdString();
  // 行内公式不加 display 类，随文排
  EXPECT_FALSE(tag.contains(QStringLiteral("math-display")))
      << tag.toStdString();
}

TEST(FormulaRendererTest, DisplayFormulaGetsDisplayClass) {
  const QString tag = FormulaToImgTag(QStringLiteral("\\frac{a}{b}"), true);
  ASSERT_FALSE(tag.isEmpty());
  EXPECT_TRUE(tag.contains(QStringLiteral("class=\"math-display\"")))
      << tag.toStdString();
}

// 分式是"必须走图"的理由：KaTeX 的 HTML 方案在 qlitehtml 下排不动它，
// 而 SVG 路线不挑内容——上下标、根号、求和都该出得来
TEST(FormulaRendererTest, HandlesStructuredFormulas) {
  const QStringList texes = {
      QStringLiteral("\\sum_{i=1}^{n} x_i^2"),
      QStringLiteral("\\int_{-\\infty}^{\\infty} e^{-x^2}\\,dx = \\sqrt{\\pi}"),
      QStringLiteral(
          "\\nabla \\cdot \\mathbf{E} = \\frac{\\rho}{\\varepsilon_0}"),
  };
  for (const QString& tex : texes) {
    EXPECT_FALSE(FormulaToImgTag(tex, true).isEmpty()) << tex.toStdString();
  }
}

// 结果按 (tex, display) 缓存：同一个公式再来一次应逐字相同（也顺带证明缓存
// 没把 display 与非 display 混在一起）
TEST(FormulaRendererTest, CachesByTexAndDisplay) {
  const QString first =
      FormulaToImgTag(QStringLiteral("a^2 + b^2 = c^2"), false);
  const QString second =
      FormulaToImgTag(QStringLiteral("a^2 + b^2 = c^2"), false);
  ASSERT_FALSE(first.isEmpty());
  EXPECT_EQ(first, second);
  EXPECT_NE(first, FormulaToImgTag(QStringLiteral("a^2 + b^2 = c^2"), true));
}

// 畸形输入不抛、不崩。约定是"空串 = 渲不出来，调用方退回显示原文"，但别指望
// 畸形一定返回空：MathJax 相当宽容（`\frac{` 会渲成缺参数的分数、`$$$` 直接
// 当字面量），真正返回空的是载荷缺失、JS 异常、SVG 无效这类内部失败。
// 所以这里只钉不变量：要么空，要么是个well-formed 的 img
TEST(FormulaRendererTest, MalformedTexNeverCrashes) {
  const QStringList bad = {QStringLiteral("\\frac{"), QStringLiteral("$$$"),
                           QStringLiteral("\\begin{matrix}"),
                           QStringLiteral("{}{}{")};
  for (const QString& tex : bad) {
    const QString tag = FormulaToImgTag(tex, false);
    const bool well_formed =
        tag.startsWith(QStringLiteral("<img")) &&
        tag.contains(QStringLiteral("data:image/png;base64,"));
    EXPECT_TRUE(tag.isEmpty() || well_formed) << tex.toStdString();
  }
}

}  // namespace
