#ifndef FILE_EXAMPLES_MD_VIEWER_RENDER_STATUS_PANEL_H_
#define FILE_EXAMPLES_MD_VIEWER_RENDER_STATUS_PANEL_H_

#include <QWidget>

class QLabel;
class MarkdownView;

// 渲染状态面板：三个引擎的加载状态与各上下文的渲染/命中计数。
// 只读展示 + 刷新/清零；文档重建时自动刷新（跟着 outlineChanged 走）
class RenderStatusPanel : public QWidget {
  Q_OBJECT

 public:
  explicit RenderStatusPanel(MarkdownView* view, QWidget* parent = nullptr);

 signals:
  void statusMessage(const QString& message);

 private:
  void RefreshStatus();

  MarkdownView* view_ = nullptr;
  QLabel* highlight_ = nullptr;
  QLabel* formula_ = nullptr;
  QLabel* mermaid_ = nullptr;
};

#endif  // FILE_EXAMPLES_MD_VIEWER_RENDER_STATUS_PANEL_H_
