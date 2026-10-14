#ifndef FILE_MARKDOWN_MARKDOWN_WINDOW_H_
#define FILE_MARKDOWN_MARKDOWN_WINDOW_H_

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;
class QUrl;
class MarkdownView;

// 文档查看器窗口：一窗一篇文档，自带"打开"按钮与拖拽。
// 链接策略在这里：本地 .md 在同一窗口打开，其余交给系统浏览器。
class MarkdownWindow : public QDialog {
  Q_OBJECT

 public:
  explicit MarkdownWindow(QWidget* parent = nullptr);

  // 载入并显示（非模态）。打不开时窗口照开，视图内给出原因
  void OpenFile(const QString& path);
  // 弹文件对话框选一篇打开。菜单项与窗口里的「打开…」按钮共用
  void OpenFileDialog();

 protected:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  void initUi();
  void initSignals();
  void OnLinkClicked(const QUrl& url);
  void ShowDocument(const QString& path);

  MarkdownView* view_ = nullptr;
  QLabel* path_label_ = nullptr;
  QPushButton* open_button_ = nullptr;
};

#endif  // FILE_MARKDOWN_MARKDOWN_WINDOW_H_
