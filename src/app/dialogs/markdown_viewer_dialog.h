#ifndef FILE_APP_MARKDOWN_VIEWER_DIALOG_H_
#define FILE_APP_MARKDOWN_VIEWER_DIALOG_H_

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;
class MarkdownView;

// Base env integration demo: a modeless Markdown viewer backed by the
// qlitemd::markdown_view component. The dialog starts with a built-in
// sample so the ribbon entry can be verified without a file dialog.
class MarkdownViewerDialog : public QDialog {
  Q_OBJECT

 public:
  explicit MarkdownViewerDialog(QWidget* parent = nullptr);

  // Loads a Markdown file. Also used by the --markdown command-line path.
  void OpenFile(const QString& path);

 private slots:
  void onOpenFile();

 private:
  void initUi();
  void initSignals();

  MarkdownView* view_ = nullptr;
  QLabel* path_label_ = nullptr;
  QPushButton* open_button_ = nullptr;
};

#endif  // FILE_APP_MARKDOWN_VIEWER_DIALOG_H_
