#include <gtest/gtest.h>

#include <QFile>
#include <QProcess>
#include <QString>
#include <QTemporaryDir>

// qjsc 是 EXCLUDE_FROM_ALL 目标，测试目标通过 add_dependencies 把它带上：
// 构建测试就会验证"补丁后 qjs-libc 能在当前工具链下编译、qjsc 能链出来"。
// 这里再跑一遍真实编译，确认生成的是合法的 C 文件而不是空壳。
TEST(QjscTest, CompilesScriptToC) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  const QString js_path = dir.filePath(QStringLiteral("in.js"));
  QFile js(js_path);
  ASSERT_TRUE(js.open(QIODevice::WriteOnly | QIODevice::Text));
  js.write("console.log(1 + 2);\n");
  js.close();

  const QString out_path = dir.filePath(QStringLiteral("out.c"));
  QProcess process;
  process.start(QString::fromUtf8(QJSC_PATH),
                {QStringLiteral("-e"), QStringLiteral("-o"), out_path,
                 js_path});
  ASSERT_TRUE(process.waitForFinished(30000))
      << "qjsc 超时：" << process.errorString().toStdString();
  EXPECT_EQ(process.exitStatus(), QProcess::NormalExit);
  EXPECT_EQ(process.exitCode(), 0)
      << process.readAllStandardError().toStdString();

  QFile out(out_path);
  ASSERT_TRUE(out.open(QIODevice::ReadOnly));
  const QString generated = QString::fromUtf8(out.readAll());
  EXPECT_TRUE(generated.contains(QStringLiteral("QuickJS-ng compiler")))
      << generated.toStdString();
  EXPECT_TRUE(generated.contains(QStringLiteral("qjsc_")))
      << generated.toStdString();
}
