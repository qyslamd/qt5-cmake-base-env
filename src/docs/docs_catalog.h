#ifndef FILE_DOCS_DOCS_CATALOG_H_
#define FILE_DOCS_DOCS_CATALOG_H_

#include <QString>
#include <QVector>

struct DocEntry {
  QString title;
  QString category;
  QString relative_path;
  QString absolute_path;
};

// Reads docs/manifest.json and validates that every document stays inside the
// docs root. A missing manifest falls back to a recursive Markdown scan.
class DocsCatalog {
 public:
  bool Load(const QString& root);

  const QString& root() const { return root_; }
  const QVector<DocEntry>& entries() const { return entries_; }
  const QString& error() const { return error_; }

  bool Contains(const QString& absolute_path) const;

 private:
  bool LoadManifest(const QString& manifest_path);
  void ScanRoot();
  bool AddEntry(const QString& relative_path, const QString& title,
                const QString& category);

  QString root_;
  QString canonical_root_;
  QVector<DocEntry> entries_;
  QString error_;
};

#endif  // FILE_DOCS_DOCS_CATALOG_H_
