#ifndef FILE_APP_QTTESTING_QTTESTING_XML_H_
#define FILE_APP_QTTESTING_QTTESTING_XML_H_

// QtTesting
#include "pqEventObserver.h"
#include "pqEventSource.h"
#include "pqEventTypes.h"

// Qt
#include <QByteArray>
#include <QFile>
#include <QString>
#include <QTextStream>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QtDebug>

// -----------------------------------------------------------------------------
// AppQtTestingXMLEventObserver
//   把 QtTesting 录制的高层事件序列化为 XML 文件。
//   基类 pqEventObserver，覆写 setStream() 与 onRecordEvent()。
// -----------------------------------------------------------------------------

class AppQtTestingXMLEventObserver : public pqEventObserver {
  QXmlStreamWriter* XMLStream;
  QString XMLString;

 public:
  explicit AppQtTestingXMLEventObserver(QObject* p = nullptr)
      : pqEventObserver(p), XMLStream(nullptr) {}

  ~AppQtTestingXMLEventObserver() override { delete this->XMLStream; }

 protected:
  void setStream(QTextStream* stream) override {
    if (this->XMLStream) {
      this->XMLStream->writeEndElement();
      this->XMLStream->writeEndDocument();
      delete this->XMLStream;
      this->XMLStream = nullptr;
    }
    if (this->Stream) {
      *this->Stream << this->XMLString;
    }
    this->XMLString = QString();
    pqEventObserver::setStream(stream);
    if (this->Stream) {
      this->XMLStream = new QXmlStreamWriter(&this->XMLString);
      this->XMLStream->setAutoFormatting(true);
      this->XMLStream->writeStartDocument();
      this->XMLStream->writeStartElement("events");
    }
  }

  void onRecordEvent(const QString& widget, const QString& command,
                     const QString& arguments, const int& eventType) override {
    if (!this->XMLStream) {
      return;
    }
    this->XMLStream->writeStartElement("event");
    this->XMLStream->writeAttribute("widget", widget);
    // 动作事件记 command，校验事件记 property：回放端据此区分（见下方
    // AppQtTestingXMLEventSource::getNextEvent），两处必须对称
    if (eventType == pqEventTypes::ACTION_EVENT) {
      this->XMLStream->writeAttribute("command", command);
    } else  // pqEventTypes::CHECK_EVENT
    {
      this->XMLStream->writeAttribute("property", command);
    }
    this->XMLStream->writeAttribute("arguments", arguments);
    this->XMLStream->writeEndElement();
  }
};

// -----------------------------------------------------------------------------
// AppQtTestingXMLEventSource
//   从 XML 文件读取事件，供 pqEventDispatcher 回放。
//   基类 pqEventSource，覆写 setContent() 与 getNextEvent()。
// -----------------------------------------------------------------------------

class AppQtTestingXMLEventSource : public pqEventSource {
  using Superclass = pqEventSource;
  QXmlStreamReader* XMLStream;

 public:
  explicit AppQtTestingXMLEventSource(QObject* p = nullptr)
      : Superclass(p), XMLStream(nullptr) {}

  ~AppQtTestingXMLEventSource() override { delete this->XMLStream; }

 protected:
  void setContent(const QString& xmlfilename) override {
    delete this->XMLStream;
    this->XMLStream = nullptr;

    QFile xml(xmlfilename);
    if (!xml.open(QIODevice::ReadOnly)) {
      qDebug() << "Failed to load " << xmlfilename;
      return;
    }
    QByteArray data = xml.readAll();
    this->XMLStream = new QXmlStreamReader(data);
    if (this->XMLStream->atEnd()) {
      qDebug() << "Invalid xml";
    }
  }

  int getNextEvent(QString& widget, QString& command, QString& arguments,
                   int& eventType) override {
    if (!this->XMLStream || this->XMLStream->atEnd()) {
      return DONE;
    }
    while (!this->XMLStream->atEnd()) {
      const QXmlStreamReader::TokenType token = this->XMLStream->readNext();
      if (token == QXmlStreamReader::StartElement &&
          this->XMLStream->name() == QStringLiteral("event")) {
        break;
      }
    }
    if (this->XMLStream->atEnd()) {
      return DONE;
    }

    const QXmlStreamAttributes attrs = this->XMLStream->attributes();
    widget = attrs.value("widget").toString();
    arguments = attrs.value("arguments").toString();
    // 与录制端对称：property 属性代表校验事件。上游示例在这里只读 command
    // 且把类型写死成 ACTION_EVENT，导致录制下来的校验事件回放时变成一条
    // 空命令的动作事件（既不报错也不生效）
    if (attrs.hasAttribute(QStringLiteral("property"))) {
      eventType = pqEventTypes::CHECK_EVENT;
      command = attrs.value("property").toString();
    } else {
      eventType = pqEventTypes::ACTION_EVENT;
      command = attrs.value("command").toString();
    }
    return SUCCESS;
  }
};

#endif  // FILE_APP_QTTESTING_QTTESTING_XML_H_
