#include "docs_catalog.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTextStream>

namespace {

QString ReadFirstHeading(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  const QString text = QString::fromUtf8(file.readAll());
  const QStringList lines = text.split(QLatin1Char('\n'));
  for (const QString& line : lines) {
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith(QLatin1String("# "))) {
      return trimmed.mid(2).trimmed();
    }
  }
  return {};
}

bool IsInsideRoot(const QString& canonical_root, const QString& path) {
  if (canonical_root.isEmpty() || path.isEmpty()) {
    return false;
  }
#ifdef Q_OS_WIN
  const Qt::CaseSensitivity kCase = Qt::CaseInsensitive;
#else
  const Qt::CaseSensitivity kCase = Qt::CaseSensitive;
#endif
  return path.compare(canonical_root, kCase) == 0 ||
         path.startsWith(canonical_root + QLatin1Char('/'), kCase);
}

}  // namespace

bool DocsCatalog::Load(const QString& root) {
  entries_.clear();
  error_.clear();
  root_ = QFileInfo(root).absoluteFilePath();
  canonical_root_ = QFileInfo(root_).canonicalFilePath();
  if (canonical_root_.isEmpty()) {
    error_ = QStringLiteral("文档目录不存在: %1").arg(root_);
    return false;
  }

  const QFileInfo manifest(
      QDir(root_).filePath(QStringLiteral("manifest.json")));
  if (manifest.isFile()) {
    if (!LoadManifest(manifest.absoluteFilePath())) {
      return false;
    }
  } else {
    ScanRoot();
  }
  if (entries_.isEmpty() && error_.isEmpty()) {
    error_ = QStringLiteral("文档目录里没有 Markdown 文件");
  }
  return !entries_.isEmpty();
}

bool DocsCatalog::LoadManifest(const QString& manifest_path) {
  QFile file(manifest_path);
  if (!file.open(QIODevice::ReadOnly)) {
    error_ = QStringLiteral("无法读取文档清单: %1").arg(manifest_path);
    return false;
  }
  QJsonParseError parse_error{};
  const QJsonDocument document =
      QJsonDocument::fromJson(file.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
    error_ =
        QStringLiteral("文档清单格式错误: %1").arg(parse_error.errorString());
    return false;
  }
  const QJsonArray documents =
      document.object().value(QStringLiteral("documents")).toArray();
  for (const QJsonValue& value : documents) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject object = value.toObject();
    const QString relative = object.value(QStringLiteral("file")).toString();
    if (relative.isEmpty()) {
      continue;
    }
    AddEntry(relative, object.value(QStringLiteral("title")).toString(),
             object.value(QStringLiteral("category")).toString());
  }
  return true;
}

void DocsCatalog::ScanRoot() {
  QDirIterator iterator(
      root_, {QStringLiteral("*.md"), QStringLiteral("*.markdown")},
      QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
  while (iterator.hasNext()) {
    const QString path = iterator.next();
    const QString relative = QDir(root_).relativeFilePath(path);
    const QString title = ReadFirstHeading(path);
    const QString folder = QFileInfo(relative).path();
    const QString category =
        folder == QLatin1String(".") ? QStringLiteral("文档") : folder;
    AddEntry(relative, title, category);
  }
}

bool DocsCatalog::AddEntry(const QString& relative_path, const QString& title,
                           const QString& category) {
  const QString cleaned = QDir::cleanPath(relative_path);
  if (cleaned.isEmpty() || QDir::isAbsolutePath(cleaned) ||
      cleaned == QLatin1String("..") ||
      cleaned.startsWith(QLatin1String("../"))) {
    return false;
  }
  const QString absolute =
      QFileInfo(QDir(root_).filePath(cleaned)).canonicalFilePath();
  if (absolute.isEmpty() || !Contains(absolute)) {
    return false;
  }
  DocEntry entry;
  entry.title = title.isEmpty() ? QFileInfo(cleaned).completeBaseName() : title;
  entry.category = category.isEmpty() ? QStringLiteral("文档") : category;
  entry.relative_path = cleaned;
  entry.absolute_path = absolute;
  entries_.append(entry);
  return true;
}

bool DocsCatalog::Contains(const QString& absolute_path) const {
  if (absolute_path.isEmpty()) {
    return false;
  }
  const QString canonical = QFileInfo(absolute_path).canonicalFilePath();
  return IsInsideRoot(canonical_root_, canonical);
}
