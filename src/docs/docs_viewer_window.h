#ifndef FILE_DOCS_DOCS_VIEWER_WINDOW_H_
#define FILE_DOCS_DOCS_VIEWER_WINDOW_H_

#include <QDialog>
#include <QString>
#include <QUrl>

#include "docs_catalog.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class MarkdownOutlineWidget;
class MarkdownView;

// 应用内文档中心：左侧文档列表与当前文档大纲，右侧 MarkdownView。
// 文档来自 <app>/docs，按 manifest.json 排序；没有清单时递归扫描 Markdown。
class DocsViewerWindow : public QDialog {
  Q_OBJECT

 public:
  explicit DocsViewerWindow(QWidget* parent = nullptr);

 private slots:
  void onDocumentRowChanged(int row);
  void onHeadingActivated(const QString& anchor);
  void onLinkClicked(const QUrl& url);
  void onFindNext();
  void onFindPrevious();
  void onZoomIn();
  void onZoomOut();
  void onZoomReset();
  void onExportHtml();
  void onPrint();
  void onRenderFinished(quint64 request_id);

 private:
  void initUi();
  void initSignals();
  void populateDocuments();
  void openEntry(const DocEntry& entry, const QString& anchor = QString());
  void showStatus(const QString& text);
  bool resolveDocumentLink(const QUrl& url, QString* path,
                           QString* anchor) const;
  const DocEntry* findEntry(const QString& absolute_path) const;

  DocsCatalog catalog_;
  QListWidget* doc_list_ = nullptr;
  MarkdownOutlineWidget* outline_ = nullptr;
  MarkdownView* view_ = nullptr;
  QLineEdit* find_edit_ = nullptr;
  QLabel* status_label_ = nullptr;
  QPushButton* find_previous_button_ = nullptr;
  QPushButton* find_next_button_ = nullptr;
  QPushButton* zoom_out_button_ = nullptr;
  QPushButton* zoom_in_button_ = nullptr;
  QPushButton* zoom_reset_button_ = nullptr;
  QPushButton* export_button_ = nullptr;
  QPushButton* print_button_ = nullptr;
  QString current_path_;
  QString pending_anchor_;
  bool updating_selection_ = false;
};

#endif  // FILE_DOCS_DOCS_VIEWER_WINDOW_H_
