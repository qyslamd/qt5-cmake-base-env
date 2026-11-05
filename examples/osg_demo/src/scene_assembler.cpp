#include "scene_assembler.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <osg/Math>
#include <osg/MatrixTransform>
#include <osgDB/ReadFile>

namespace {

class GeodeNameCollector : public osg::NodeVisitor {
 public:
  GeodeNameCollector()
      : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {}

  void apply(osg::Geode& geode) override {
    if (!geode.getName().empty()) {
      names_ << QString::fromStdString(geode.getName());
    }
  }

  QStringList names_;
};

osg::Vec3d ReadVec3(const QJsonArray& arr, const osg::Vec3d& fallback) {
  if (arr.size() != 3) {
    return fallback;
  }
  return osg::Vec3d(arr[0].toDouble(), arr[1].toDouble(), arr[2].toDouble());
}

}  // namespace

bool SceneAssembler::Load(const QString& config_path, QString* error) {
  root_ = new osg::Group;
  scene_name_.clear();

  QFile file(config_path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error != nullptr) {
      *error = QStringLiteral("Cannot open config: %1").arg(config_path);
    }
    return false;
  }

  QJsonParseError parse_error{};
  const QJsonDocument doc =
      QJsonDocument::fromJson(file.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError) {
    if (error != nullptr) {
      *error = QStringLiteral("Invalid JSON (offset %1): %2")
                   .arg(parse_error.offset)
                   .arg(parse_error.errorString());
    }
    return false;
  }

  const QJsonObject obj = doc.object();
  scene_name_ = obj.value("name").toString(QStringLiteral("Scene"));

  const QDir base_dir = QFileInfo(config_path).absoluteDir();
  const QJsonArray parts = obj.value("parts").toArray();
  if (parts.isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("Config has no parts: %1").arg(config_path);
    }
    return false;
  }

  int loaded = 0;
  for (const QJsonValue& v : parts) {
    const QJsonObject part = v.toObject();
    PartSpec spec;
    const QString rel = part.value("file").toString();
    if (rel.isEmpty()) {
      qWarning() << "Part has no 'file' field, skipped";
      continue;
    }
    spec.file_path = QDir::cleanPath(base_dir.absoluteFilePath(rel));
    spec.position = ReadVec3(part.value("position").toArray(), {0.0, 0.0, 0.0});
    spec.rotation_deg =
        ReadVec3(part.value("rotation_deg").toArray(), {0.0, 0.0, 0.0});

    QString part_error;
    if (!LoadPart(spec, &part_error)) {
      qWarning() << "Failed to load part:" << spec.file_path << part_error;
      continue;
    }
    ++loaded;
  }

  if (loaded == 0) {
    if (error != nullptr) {
      *error = QStringLiteral("No parts loaded from: %1").arg(config_path);
    }
    return false;
  }

  qInfo() << "[Scene]" << scene_name_ << "loaded parts:" << loaded << "/"
          << parts.size();
  return true;
}

bool SceneAssembler::LoadPart(const PartSpec& spec, QString* error) {
  if (!QFile::exists(spec.file_path)) {
    if (error != nullptr) {
      *error = QStringLiteral("File not found: %1").arg(spec.file_path);
    }
    return false;
  }

  osg::ref_ptr<osg::Node> node =
      osgDB::readNodeFile(spec.file_path.toUtf8().constData());
  if (!node.valid()) {
    if (error != nullptr) {
      *error = QStringLiteral("OSG failed to read: %1").arg(spec.file_path);
    }
    return false;
  }

  osg::Matrix m = osg::Matrix::rotate(osg::PI, osg::Vec3d(1.0, 0.0, 0.0));
  if (spec.rotation_deg.x() != 0.0) {
    m.postMult(osg::Matrix::rotate(osg::DegreesToRadians(spec.rotation_deg.x()),
                                   osg::X_AXIS));
  }
  if (spec.rotation_deg.y() != 0.0) {
    m.postMult(osg::Matrix::rotate(osg::DegreesToRadians(spec.rotation_deg.y()),
                                   osg::Y_AXIS));
  }
  if (spec.rotation_deg.z() != 0.0) {
    m.postMult(osg::Matrix::rotate(osg::DegreesToRadians(spec.rotation_deg.z()),
                                   osg::Z_AXIS));
  }
  m.postMult(osg::Matrix::translate(spec.position));

  osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform;
  mt->setMatrix(m);
  mt->addChild(node);
  mt->setName(QFileInfo(spec.file_path).completeBaseName().toStdString());

  // === 新增日志：打印零件落位信息 ===
  const osg::BoundingSphere bs_raw = node->getBound();
  const osg::BoundingSphere bs_final = mt->getBound();
  qInfo() << "[Part]" << QFileInfo(spec.file_path).fileName() << "json_pos=("
          << spec.position.x() << "," << spec.position.y() << ","
          << spec.position.z() << ")"
          << "final_center=(" << bs_final.center().x() << ","
          << bs_final.center().y() << "," << bs_final.center().z() << ")";

  root_->addChild(mt);
  return true;
}

QStringList SceneAssembler::GetComponentNames() const {
  if (!root_.valid()) {
    return {};
  }
  GeodeNameCollector collector;
  root_->accept(collector);

  QStringList unique;
  QSet<QString> seen;
  for (const QString& n : collector.names_) {
    if (!seen.contains(n)) {
      seen.insert(n);
      unique << n;
    }
  }
  unique.sort();
  return unique;
}
