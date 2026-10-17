#include "mermaid_renderer.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QString>
#include <QStringList>

#include "markdown_render_context.h"
#include "markdown_types.h"

// 端到端判据：qrc 载荷 → QuickJS 里跑 mermaid → SVG 后处理 → QSvgRenderer 画成
// PNG → 内联成 <img>。中间任何一环坏了都只能从"产物本身"看出来，所以在无头环境
// 把产物断言掉。
//
// 要 QGuiApplication 而不是 QCoreApplication：QSvgRenderer 走 QPainter，且度量
// 要查 Qt 字体库。
//
// **offscreen 平台没有字体库**（实测 QFontDatabase::families() 返回 0 个族）：
// 于是文字会被画成 notdef 方框、选族名也会落空。所以这里只断言"图出来了、是
// well-formed 的 <img>"，**不拿文字像素做判据**——像素验收要走真实平台（App +
// funq 抓图），那条路字体库是全的。
//
// 首次调用要 eval 3.5MB 的 bundle（秒级），用例超时在 CMake 里放宽到 120 秒
int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
  QGuiApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

namespace {

TEST(MermaidRendererTest, RendersFlowchartToInlineImage) {
  ASSERT_TRUE(MermaidRendererReady()) << "qrc 里的三份载荷没加载起来";
  const QString tag = MermaidToImgTag(
      QStringLiteral("flowchart TD\n    A[开始] --> B[结束]\n"));
  ASSERT_FALSE(tag.isEmpty()) << "渲染失败（会退回代码块显示）";
  EXPECT_TRUE(tag.startsWith(QStringLiteral("<img"))) << tag.toStdString();
  EXPECT_TRUE(tag.contains(QStringLiteral("class=\"mermaid\"")))
      << tag.toStdString();
  EXPECT_TRUE(tag.contains(QStringLiteral("data:image/png;base64,")))
      << tag.toStdString();
}

// 另一条代码路径：sequence 的消息线是 <line> 而不是 <path>，箭头能不能出来
// 全看后处理有没有把它一起覆盖
TEST(MermaidRendererTest, RendersSequenceDiagramToInlineImage) {
  const QString tag = MermaidToImgTag(QStringLiteral(
      "sequenceDiagram\n    Alice->>Bob: hi\n    Bob-->>Alice: ok\n"));
  ASSERT_FALSE(tag.isEmpty());
  EXPECT_TRUE(tag.contains(QStringLiteral("data:image/png;base64,")));
}

// 结果按源码缓存：同一段源码再来一次应逐字相同。
// 换一段源码要比出不同**不能只改标签文字**——offscreen 下文字是方框、宽度也
// 一样，两张图会逐字节相同；改节点数量才是与字体无关的差异
TEST(MermaidRendererTest, CachesBySource) {
  const QString source = QStringLiteral("flowchart LR\n    X --> Y\n");
  const QString first = MermaidToImgTag(source);
  const QString second = MermaidToImgTag(source);
  ASSERT_FALSE(first.isEmpty());
  EXPECT_EQ(first, second);
  EXPECT_NE(first, MermaidToImgTag(
                       QStringLiteral("flowchart LR\n    X --> Y --> Z\n")));
}

// 回归点：mermaid.render 走 .catch（图示类型都认不出来）时也必须置 C 侧
// settled 旗子。漏了的话泵循环要跑满二十万次 job 或 30 秒上限才放弃，一张
// 坏图能把界面卡半分钟——比空结果本身严重得多
TEST(MermaidRendererTest, RenderErrorSettlesPromptly) {
  ASSERT_TRUE(MermaidRendererReady());
  QElapsedTimer clock;
  clock.start();
  const QString tag =
      MermaidToImgTag(QStringLiteral("notadiagram\n    A --> B\n"));
  const qint64 elapsed = clock.elapsed();
  EXPECT_TRUE(tag.isEmpty()) << "这类输入应当被 mermaid 拒绝";
  EXPECT_LT(elapsed, 5000) << "出错路径没有及时落定，泵循环跑满了预算";
}

// 上下文隔离：每个视图一份缓存与计数，清一个不影响另一个
TEST(MermaidRendererTest, ContextsKeepCachesSeparate) {
  MarkdownRenderContext first;
  MarkdownRenderContext second;
  const QString source = QStringLiteral("flowchart LR\n    A --> B\n");
  const QString tag = MermaidToImgTag(first, source, MarkdownTheme::Light);
  ASSERT_FALSE(tag.isEmpty());
  EXPECT_EQ(first.status().mermaid_context.renders, 1U);
  EXPECT_EQ(second.status().mermaid_context.renders, 0U);

  EXPECT_EQ(MermaidToImgTag(second, source, MarkdownTheme::Light), tag);
  EXPECT_EQ(second.status().mermaid_context.renders, 1U);

  EXPECT_EQ(MermaidToImgTag(first, source, MarkdownTheme::Light), tag);
  EXPECT_EQ(first.status().mermaid_context.memory_cache_hits, 1U);

  first.ClearMemoryCaches();
  EXPECT_EQ(MermaidToImgTag(first, source, MarkdownTheme::Light), tag);
  EXPECT_EQ(first.status().mermaid_context.renders, 2U);

  EXPECT_EQ(MermaidToImgTag(second, source, MarkdownTheme::Light), tag);
  EXPECT_EQ(second.status().mermaid_context.memory_cache_hits, 1U);
}

// 畸形输入不抛、不崩。约定是"空串 = 渲不出来，调用方退回代码块"，但别指望畸形
// 一定返回空：mermaid 对某些写法相当宽容（会画出一张降级图）。所以这里只钉
// 不变量：要么空，要么是个 well-formed 的 img
TEST(MermaidRendererTest, MalformedCodeNeverCrashes) {
  const QStringList bad = {
      QStringLiteral("flowchart TD\n    A[\n"),
      QStringLiteral("sequenceDiagram\n    ???\n"),
      QStringLiteral("notadiagram\n    A --> B\n"),
      QString(),
  };
  for (const QString& code : bad) {
    const QString tag = MermaidToImgTag(code);
    const bool well_formed =
        tag.startsWith(QStringLiteral("<img")) &&
        tag.contains(QStringLiteral("data:image/png;base64,"));
    EXPECT_TRUE(tag.isEmpty() || well_formed) << code.toStdString();
  }
}

}  // namespace
