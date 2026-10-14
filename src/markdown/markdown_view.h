#ifndef FILE_MARKDOWN_MARKDOWN_VIEW_H_
#define FILE_MARKDOWN_MARKDOWN_VIEW_H_

#include <QString>
#include <QUrl>
#include <QWidget>

class QFileSystemWatcher;
class QLiteHtmlWidget;

// Markdown 预览控件：加载 .md、套内置样式渲染、跟随文件变化重载。
// 只做"渲染"这一件事——链接点击原样抛出，本地 .md 是自开还是交给系统浏览器
// 是宿主的策略；qlitehtml 的类型不出现在本头文件里，换渲染后端不动调用方。
class MarkdownView : public QWidget {
  Q_OBJECT

 public:
  explicit MarkdownView(QWidget* parent = nullptr);
  ~MarkdownView() override;

  // 从磁盘加载，成功返回 true。基准 URL 取文件所在目录，相对路径的图片与
  // 链接据此解析；失败时视图内提示原因，并回到"没有文档"的状态
  bool LoadFile(const QString& path);
  // 直接给文本。基准 URL 就是 base_dir（空 = 没有基准），不给上一份文档的目录
  // 留后门——否则相对图片会悄悄按上次那个目录解析
  void SetContent(const QString& markdown, const QString& base_dir);
  QString current_file() const { return current_file_; }
  bool has_document() const { return !current_file_.isEmpty(); }

  // 滚到页内锚点。md4c 不给标题加 id，找不到目标时原地不动
  void ScrollToAnchor(const QString& anchor);

 signals:
  void linkClicked(const QUrl& url);
  void loadFailed(const QString& path, const QString& reason);

 private:
  void initUi();
  void initSignals();
  void ApplyDocument();
  void ShowMessage(const QString& text);
  void ClearDocument();
  void WatchCurrentFile();
  void OnFileChanged();
  void OnReloadTick();

  QLiteHtmlWidget* html_ = nullptr;
  QFileSystemWatcher* watcher_ = nullptr;
  // "先删后建"保存时用来等文件回来，见 OnFileChanged
  QTimer* reload_timer_ = nullptr;
  int reload_attempts_ = 0;
  QString markdown_;
  QString current_file_;
  QString base_dir_;
};

#endif  // FILE_MARKDOWN_MARKDOWN_VIEW_H_
