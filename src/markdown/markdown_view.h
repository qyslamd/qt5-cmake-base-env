#ifndef FILE_MARKDOWN_MARKDOWN_VIEW_H_
#define FILE_MARKDOWN_MARKDOWN_VIEW_H_

#include <QHash>
#include <QString>
#include <QUrl>
#include <QWidget>

class QFileSystemWatcher;
class QLiteHtmlWidget;
class QPrinter;

// copy: 链接（代码块"复制"按钮）→ 要复制的原文；不是 copy: 或表里查不到
// 返回空串。抽成自由函数是为了可测：物理点击 funq 驱动不了（html 是
// qlitehtml 自绘的，合成鼠标事件到不了它的链接命中判定），而"信号之后"的
// 拦截与查表是本模块的逻辑。空串 = 别动剪贴板（宁可没反应，也不能把用户
// 剪贴板里原来的东西清掉）
QString CopyTextForLink(const QUrl& url, const QHash<QString, QString>& blocks);

// Markdown 预览控件：加载 .md、套内置样式渲染、跟随文件变化重载。
// 只做"渲染"这一件事——链接点击原样抛出（copy: 的代码块复制链接除外，那
// 是控件内部能力：查表放剪贴板，见 ApplyDocument）；本地 .md 是自开还是
// 交给系统浏览器是宿主的策略；qlitehtml 的类型不出现在本头文件里，换渲染
// 后端不动调用方。
//
// 内置交互：Ctrl+滚轮 / Ctrl+= / Ctrl+- 缩放（0.3~5.0）、Ctrl+0 复位、
// Ctrl+C 复制选中文本（QAbstractScrollArea 的默认按键处理会吞掉 Copy，
// 必须拦）。
class MarkdownView : public QWidget {
  Q_OBJECT

 public:
  // 查找选项。自定义而不用 QTextDocument::FindFlags——宿主不该为了几个
  // 枚举值把 QTextDocument 拉进编译单元
  enum FindFlag {
    FindNoFlag = 0,
    FindBackward = 1,
    FindCaseSensitively = 2,
    FindWholeWords = 4,
  };
  Q_DECLARE_FLAGS(FindFlags, FindFlag)
  explicit MarkdownView(QWidget* parent = nullptr);
  ~MarkdownView() override;

  // 从磁盘加载，成功返回 true。基准 URL 取文件所在目录，相对路径的图片与
  // 链接据此解析；失败时视图内提示原因，并回到"没有文档"的状态
  bool LoadFile(const QString& path);
  // 直接给文本。基准 URL 就是 base_dir（空 = 没有基准），不给上一份文档的目录
  // 留后门——否则相对图片会悄悄按上次那个目录解析
  void SetContent(const QString& markdown, const QString& base_dir);
  QString current_file() const { return current_file_; }
  // 两种载入都算"有文档"：LoadFile 落 current_file_，SetContent 只落
  // markdown_——只查后者的话文本直载后这里恒为 false
  bool has_document() const {
    return !current_file_.isEmpty() || !markdown_.isEmpty();
  }

  // 滚到页内锚点：用户写的是 slug（`#标题`），这里经内部的 slug→id 表换成
  // 元素再滚；查不到就原样透传（手写的 <a name="x"> 那类归 litehtml 自己找）
  void ScrollToAnchor(const QString& anchor);

  // 查找并选中命中处（找到会滚过去）。wrapped 非空时被写成"本次是否绕回了
  // 开头"（每次调用都会写——控件本体只在绕回时写，见实现），宿主拿它提示
  // "已到末尾"。返回 false = 没有命中，选区不动
  bool FindText(const QString& text, FindFlags flags, bool* wrapped = nullptr);
  // 打印/导出 PDF：QPrinter 由宿主自备（本控件不碰文件系统与输出格式）。
  // 控件按 print 媒体重排一遍再逐页绘制——分页是按页高切片的，跨页的长
  // 元素会被切在页缝上
  void Print(QPrinter* printer);

 signals:
  void linkClicked(const QUrl& url);
  void loadFailed(const QString& path, const QString& reason);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void initUi();
  void initSignals();
  void ApplyDocument();
  void ShowMessage(const QString& text);
  void ClearDocument();
  void WatchCurrentFile();
  void OnFileChanged();
  void OnReloadTick();
  void SetZoom(qreal factor);

  QLiteHtmlWidget* html_ = nullptr;
  QFileSystemWatcher* watcher_ = nullptr;
  // "先删后建"保存时用来等文件回来，见 OnFileChanged
  QTimer* reload_timer_ = nullptr;
  int reload_attempts_ = 0;
  QString markdown_;
  QString current_file_;
  QString base_dir_;
  // 代码块 id → 原始代码文本，供"复制"链接取用；每次 ApplyDocument 重建
  QHash<QString, QString> code_blocks_;
  // 标题 slug → 元素 id，供页内锚点反查；同样每次 ApplyDocument 重建
  QHash<QString, QString> heading_ids_;
  qreal zoom_ = 1.0;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MarkdownView::FindFlags)

#endif  // FILE_MARKDOWN_MARKDOWN_VIEW_H_
