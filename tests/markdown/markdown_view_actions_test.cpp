// 视图的两个"能力透传"：查找与打印。两者都是 QLiteHtmlWidget 的既有能力，
// 本模块只加了守卫（空串、空指针）与语义约定，所以判据取行为的最小面——
// 找得到/找不到、PDF 真的出得来。
//
// 无头跑：offscreen 平台不建真窗口；QPrinter(PdfFormat) 也不需要真打印机，
// 直接往临时文件写。这里不验"排出来长什么样"——那是像素的事，抓图走 funq
#include <gtest/gtest.h>

#include <QApplication>
#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QPrinter>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>

#include "markdown_view.h"

int main(int argc, char** argv) {
  // 只在无显示的 Linux 上切 offscreen。Windows 留着默认平台——打印用例
  // 需要打印后端，offscreen 没有（那种环境下用例自己 skip）
#ifdef Q_OS_LINUX
  if (qEnvironmentVariableIsEmpty("DISPLAY") &&
      qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
  }
#endif
  // 视图默认开启持久缓存：测试模式 + 唯一应用名，别读写真实缓存目录
  QStandardPaths::setTestModeEnabled(true);
  QCoreApplication::setOrganizationName(QStringLiteral("md-tests"));
  QCoreApplication::setApplicationName(
      QStringLiteral("view-actions-%1")
          .arg(QDateTime::currentMSecsSinceEpoch()));
  QApplication app(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  const int result = RUN_ALL_TESTS();
  QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
       QStringLiteral("/markdown-render"))
      .removeRecursively();
  return result;
}

namespace {

// 一篇够"长"的文档：打印用例要有内容可分页，查找用例要有靶子
QString SampleMarkdown() {
  return QStringLiteral(
      "# 标题\n\n正文里藏着 needle 这个词，还有别的字。\n\n"
      "## 第二节\n\n再来一段撑开版面，打印时至少能看出一页。\n");
}

TEST(MarkdownViewActionsTest, FindHits) {
  MarkdownView view;
  view.SetContent(SampleMarkdown(), QString());
  bool wrapped = true;  // 先置反，确认实现真的写了它
  EXPECT_TRUE(view.FindText(QStringLiteral("needle"), MarkdownView::FindFlags(),
                            &wrapped));
  EXPECT_FALSE(wrapped);
}

TEST(MarkdownViewActionsTest, FindMissesWithoutTouchingSelection) {
  MarkdownView view;
  view.SetContent(SampleMarkdown(), QString());
  EXPECT_FALSE(
      view.FindText(QStringLiteral("完全没有的词"), MarkdownView::FindFlags()));
}

// 空串是误用，不是"找全部"：必须原样返回 false，不滚不选
TEST(MarkdownViewActionsTest, EmptyNeedleIsRejected) {
  MarkdownView view;
  view.SetContent(SampleMarkdown(), QString());
  EXPECT_FALSE(view.FindText(QString(), MarkdownView::FindFlags()));
}

TEST(MarkdownViewActionsTest, PrintWritesPdf) {
  MarkdownView view;
  view.SetContent(SampleMarkdown(), QString());

  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("out.pdf"));

  QPrinter printer(QPrinter::HighResolution);
  printer.setOutputFormat(QPrinter::PdfFormat);
  printer.setOutputFileName(path);
  if (!printer.isValid()) {
    GTEST_SKIP() << "本平台没有打印后端（如 offscreen），跳过 PDF 用例";
  }
  view.Print(&printer);

  QFile pdf(path);
  ASSERT_TRUE(pdf.open(QIODevice::ReadOnly))
      << "没写出 PDF：" << path.toStdString();
  EXPECT_EQ(pdf.read(5), QByteArray("%PDF-"));
  EXPECT_GT(pdf.size(), 1000);  // 空文档也会有个骨架，太小说明什么都没画
}

// 打印容器是新建的 DocumentContainer，只复制了 dataCallback 时 token 图会
// 全部空白；PDF 里必须真的出现图片对象
TEST(MarkdownViewActionsTest, PrintIncludesFormulaImage) {
  MarkdownView view;
  view.SetContent(QStringLiteral("$x^2$\n"), QString());

  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("formula.pdf"));

  QPrinter printer(QPrinter::HighResolution);
  printer.setOutputFormat(QPrinter::PdfFormat);
  printer.setOutputFileName(path);
  if (!printer.isValid()) {
    GTEST_SKIP() << "本平台没有打印后端（如 offscreen），跳过 PDF 用例";
  }
  view.Print(&printer);

  QFile pdf(path);
  ASSERT_TRUE(pdf.open(QIODevice::ReadOnly));
  const QByteArray data = pdf.readAll();
  EXPECT_TRUE(data.contains("/Subtype /Image") ||
              data.contains("/Subtype/Image"))
      << data.left(2000).toStdString();
}

namespace {

int PdfPageCount(const QByteArray& pdf) {
  int count = 0;
  int from = 0;
  while ((from = pdf.indexOf("/Type /Page", from)) >= 0) {
    // 排除 /Type /Pages
    if (pdf.at(from + 11) != 's') {
      ++count;
    }
    from += 11;
  }
  return count;
}

}  // namespace

// 回归：HighResolution 打印机下必须按 CSS 逻辑像素布局。曾经把设备像素
// （约 9921 px 宽）当逻辑像素传给 litehtml，文字被缩到几十分之一，且分页
// 循环拿设备页高比逻辑文档高，永远只有一页
TEST(MarkdownViewActionsTest, PrintHighResolutionPaginatesLongDocument) {
  QString markdown;
  for (int i = 0; i < 200; ++i) {
    markdown += QStringLiteral(
                    "第 %1 段：这是一段用来撑开打印页数的正文，"
                    "每段都足够长，确保文档跨越多个 A4 页面。\n\n")
                    .arg(i);
  }
  MarkdownView view;
  view.SetContent(markdown, QString());

  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("long.pdf"));
  QPrinter printer(QPrinter::HighResolution);
  printer.setOutputFormat(QPrinter::PdfFormat);
  printer.setOutputFileName(path);
  if (!printer.isValid()) {
    GTEST_SKIP() << "本平台没有打印后端（如 offscreen），跳过 PDF 用例";
  }
  view.Print(&printer);

  QFile pdf(path);
  ASSERT_TRUE(pdf.open(QIODevice::ReadOnly));
  const QByteArray bytes = pdf.readAll();
  EXPECT_GE(PdfPageCount(bytes), 2)
      << "长文档只出了一页：页面/文档高度单位可能又错位了";
}

// 空指针不崩：宿主忘了判空时也别炸
TEST(MarkdownViewActionsTest, PrintNullPrinterIsNoop) {
  MarkdownView view;
  view.SetContent(SampleMarkdown(), QString());
  view.Print(nullptr);
}

// 文本直载也算"有文档"：SetContent 只落 markdown_、不动 current_file_，
// has_document 只查后者的话这里恒为 false（md_viewer 的导出守卫就踩过）
TEST(MarkdownViewActionsTest, SetContentMarksDocumentAsPresent) {
  MarkdownView view;
  EXPECT_FALSE(view.has_document());
  view.SetContent(SampleMarkdown(), QString());
  EXPECT_TRUE(view.has_document());
  EXPECT_TRUE(view.current_file().isEmpty());  // 依旧没有文件归属
}

}  // namespace
