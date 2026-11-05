#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMessageBox>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QTimer>
#include <osg/Notify>
#include <osgDB/Registry>

#include "logger.h"
#include "main_window.h"
#include "osg_widget.h"

namespace {

constexpr int kDefaultDelayMs = 1500;
constexpr int kGlDepthBits = 24;
constexpr int kGlStencilBits = 8;
constexpr int kGlSamples = 4;

void HandleQtMessage(QtMsgType type, const QMessageLogContext&,
                     const QString& msg) {
  QString level;
  switch (type) {
    case QtDebugMsg:
      level = "DEBUG";
      break;
    case QtInfoMsg:
      level = "INFO";
      break;
    case QtWarningMsg:
      level = "WARN";
      break;
    case QtCriticalMsg:
      level = "ERROR";
      break;
    case QtFatalMsg:
      level = "FATAL";
      break;
  }
  Logger::Instance()->Log(QString("[%1] %2").arg(level, msg), LogSource::kApp);
  if (type == QtFatalMsg) {
    abort();
  }
}

class OsgNotifyHandler : public osg::NotifyHandler {
 public:
  void notify(osg::NotifySeverity severity, const char* message) override {
    QString level;
    switch (severity) {
      case osg::ALWAYS:
      case osg::FATAL:
        level = "OSG_FATAL";
        break;
      case osg::WARN:
        level = "OSG_WARN";
        break;
      case osg::NOTICE:
      case osg::INFO:
        level = "OSG_INFO";
        break;
      case osg::DEBUG_INFO:
      case osg::DEBUG_FP:
        level = "OSG_DEBUG";
        break;
    }
    QString msg = QString::fromUtf8(message);
    if (msg.endsWith('\n')) {
      msg.chop(1);
    }
    if (msg.isEmpty()) {
      return;
    }
    Logger::Instance()->Log(QString("[%1] %2").arg(level, msg),
                            LogSource::kOsg);
  }
};

}  // namespace

int main(int argc, char** argv) {
  qRegisterMetaType<LogSource>("LogSource");

  QSurfaceFormat format;
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setDepthBufferSize(kGlDepthBits);
  format.setStencilBufferSize(kGlStencilBits);
  format.setSamples(kGlSamples);
  QSurfaceFormat::setDefaultFormat(format);

  QApplication app(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("OsgQtViewer"));
  QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

  // Fusion 风格下 QSS 表现更一致，避免原生控件样式覆盖
  app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

  QFile style_file(QStringLiteral(":/default.qss"));
  if (style_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    app.setStyleSheet(QString::fromUtf8(style_file.readAll()));
    style_file.close();
  } else {
    qWarning() << "Stylesheet resource not found: :/default.qss";
  }

  qInstallMessageHandler(HandleQtMessage);
  osg::setNotifyLevel(osg::INFO);
  // NotifyHandler 由 osg::ref_ptr 持有，必须堆分配；若传入静态对象，
  // 解除注册时 OSG 会对它调用 delete，导致堆损坏
  osg::setNotifyHandler(new OsgNotifyHandler);

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("OSG + Qt Model Viewer"));
  parser.addHelpOption();
  parser.addVersionOption();

  const QCommandLineOption model_option(
      QStringList() << "m" << "model",
      QStringLiteral("Scene JSON or OSG model path"), QStringLiteral("path"));
  const QCommandLineOption screenshot_option(
      QStringList() << "s" << "screenshot",
      QStringLiteral("Save screenshot and exit"), QStringLiteral("path"));
  const QCommandLineOption delay_option(
      QStringList() << "d" << "delay",
      QStringLiteral("Delay before screenshot (ms)"), QStringLiteral("ms"),
      QStringLiteral("1500"));
  const QCommandLineOption full_window_option(
      QStringList() << "f" << "full-window",
      QStringLiteral("Save a screenshot of the whole window "
                     "(including docks) instead of the 3D view only"));

  parser.addOption(model_option);
  parser.addOption(screenshot_option);
  parser.addOption(delay_option);
  parser.addOption(full_window_option);
  parser.process(app);

  QString model_path = parser.value(model_option);
  if (model_path.isEmpty()) {
    // 默认加载 exe 旁 assets/scenes 下扫描到的第一个 JSON 场景
    // （场景与模型由 CMake copy_app_assets 拷贝到可执行文件旁）
    const QDir scenes_dir(
        QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(QStringLiteral("assets/scenes")));
    const QStringList json_files = scenes_dir.entryList(
        QStringList() << QStringLiteral("*.json"), QDir::Files, QDir::Name);
    if (!json_files.isEmpty()) {
      model_path = scenes_dir.absoluteFilePath(json_files.first());
    }
  }

  const QFileInfo model_info(model_path);
  osgDB::Registry::instance()->getDataFilePathList().push_back(
      model_info.absolutePath().toUtf8().constData());

  qInfo() << "Loading model:" << model_path
          << "exists:" << QFile::exists(model_path);

  MainWindow window;
  QString error;
  if (!window.LoadModel(model_path, &error)) {
    qCritical() << "Failed to load:" << error;
    QMessageBox::critical(&window, QStringLiteral("Load Failed"), error);
    return 2;
  }

  window.show();

  const bool grab_full_window = parser.isSet(full_window_option);

  const QString screenshot_path = parser.value(screenshot_option);
  if (!screenshot_path.isEmpty()) {
    bool ok = false;
    int delay_ms = parser.value(delay_option).toInt(&ok);
    if (!ok || delay_ms < 0) {
      delay_ms = kDefaultDelayMs;
    }

    QTimer::singleShot(
        delay_ms, &window, [&window, screenshot_path, grab_full_window]() {
          const QImage image = grab_full_window
                                   ? window.grab().toImage()
                                   : window.GetOsgWidget()->GrabFramebuffer();
          if (image.isNull() || !image.save(screenshot_path, "PNG")) {
            QCoreApplication::exit(3);
            return;
          }
          QCoreApplication::exit(0);
        });
  }

  const int exit_code = app.exec();

  // 解除注册并释放 handler，避免 osg.dll 退出时 notify 回调到已销毁对象
  osg::setNotifyHandler(nullptr);

  return exit_code;
}
