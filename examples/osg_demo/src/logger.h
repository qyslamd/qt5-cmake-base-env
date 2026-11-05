#ifndef OSG_QT_VIEWER_SRC_LOGGER_H_
#define OSG_QT_VIEWER_SRC_LOGGER_H_

#include <QObject>
#include <QString>

// 消息来源，用于 UI 侧做过滤。
enum class LogSource {
  kApp,  // 业务 / Qt 层日志
  kOsg,  // OSG 底层日志（如 "Reading OBJ file"）
};

class Logger : public QObject {
  Q_OBJECT

 public:
  static Logger* Instance();

  void Log(const QString& msg, LogSource source = LogSource::kApp);

 signals:
  void MessageLogged(const QString& msg, LogSource source);

 private:
  explicit Logger(QObject* parent = nullptr) : QObject(parent) {}
};

Q_DECLARE_METATYPE(LogSource)

#endif  // OSG_QT_VIEWER_SRC_LOGGER_H_