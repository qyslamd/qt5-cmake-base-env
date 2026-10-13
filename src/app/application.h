#ifndef FILE_APP_APPLICATION_H_
#define FILE_APP_APPLICATION_H_

#include <QApplication>
#include <QStringList>
#include <QTranslator>

// 应用对象：只收"就是 QApplication 的事"——元信息、样式与全局字体、界面翻译，
// 都在构造期完成。日志、异常处理、单实例、工具模式、界面流程仍归 main 的启动
// 序列，不往这里搬。
//
// 翻译在此装载而不是 main：它必须早于任何界面构造，而 QTranslator 的持有者
// 也该是活得最久的那个对象——installTranslator 只记指针
class Application : public QApplication {
  Q_OBJECT

 public:
  Application(int& argc, char** argv);

  // 翻译装载失败项。构造期日志系统还没起（Logger::init 在 main 里），
  // 由 main 在它之后补记
  const QStringList& translationProblems() const {
    return translation_problems_;
  }

 private:
  void setupMetadata();
  void setupStyle();
  void installTranslations();

  QTranslator qt_translator_;
  QTranslator saribbon_translator_;
  QStringList translation_problems_;
};

#endif  // FILE_APP_APPLICATION_H_
