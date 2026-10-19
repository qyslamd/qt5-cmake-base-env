#include "render_status_panel.h"

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "markdown_types.h"
#include "markdown_view.h"

namespace {

QString EngineStateText(const MarkdownEngineStatus& engine) {
  if (engine.permanently_failed) {
    return QStringLiteral("加载失败");
  }
  if (!engine.loaded) {
    return QStringLiteral("未加载");
  }
  return QStringLiteral("已加载（首次 %1 ms）").arg(engine.first_load_ms);
}

QString CounterText(const MarkdownContextStatus& counters) {
  // 调用 = 实际渲染 + 两种缓存命中；只盯"渲染"会在缓存命中时永远看到 0
  const quint64 calls =
      counters.renders + counters.memory_cache_hits + counters.disk_cache_hits;
  return QStringLiteral(
             "调用 %1 · 渲染 %2 · 失败 %3 · 内存命中 %4 · 磁盘命中 %5")
      .arg(calls)
      .arg(counters.renders)
      .arg(counters.failures)
      .arg(counters.memory_cache_hits)
      .arg(counters.disk_cache_hits);
}

}  // namespace

RenderStatusPanel::RenderStatusPanel(MarkdownView* view, QWidget* parent)
    : QWidget(parent), view_(view) {
  // dock 可以被压得很矮：内容放进滚动区，别把计数器挤没
  auto* panel_layout = new QVBoxLayout(this);
  panel_layout->setContentsMargins(0, 0, 0, 0);

  auto* scroll = new QScrollArea(this);
  scroll->setObjectName(QStringLiteral("statusScroll"));
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // blur 模式下 dock 本身是半透明的，滚动区不能再自己刷一层底色
  scroll->setAutoFillBackground(false);
  scroll->setBackgroundRole(QPalette::NoRole);
  scroll->viewport()->setAutoFillBackground(false);
  scroll->viewport()->setBackgroundRole(QPalette::NoRole);

  auto* body = new QWidget;
  body->setAutoFillBackground(false);
  body->setBackgroundRole(QPalette::NoRole);
  auto* layout = new QVBoxLayout(body);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(10);

  auto* title = new QLabel(QStringLiteral("渲染状态"), body);
  QFont font = title->font();
  font.setBold(true);
  title->setFont(font);
  layout->addWidget(title);

  highlight_ = new QLabel(body);
  highlight_->setObjectName(QStringLiteral("statusHighlight"));
  formula_ = new QLabel(body);
  formula_->setObjectName(QStringLiteral("statusFormula"));
  mermaid_ = new QLabel(body);
  mermaid_->setObjectName(QStringLiteral("statusMermaid"));
  for (QLabel* label : {highlight_, formula_, mermaid_}) {
    label->setWordWrap(true);
    // 纯文本避免计数器里的分隔符被当成富文本；鼠标可选、键盘可 Ctrl+C
    label->setTextFormat(Qt::PlainText);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                   Qt::TextSelectableByKeyboard);
    label->setFocusPolicy(Qt::ClickFocus);
    layout->addWidget(label);
  }

  auto* actions = new QWidget(body);
  auto* actions_layout = new QHBoxLayout(actions);
  actions_layout->setContentsMargins(0, 0, 0, 0);
  actions_layout->setSpacing(6);
  auto* refresh = new QPushButton(QStringLiteral("刷新"), actions);
  auto* reset = new QPushButton(QStringLiteral("计数清零"), actions);
  actions_layout->addWidget(refresh);
  actions_layout->addWidget(reset);
  actions_layout->addStretch(1);
  layout->addWidget(actions);
  layout->addStretch(1);

  scroll->setWidget(body);
  panel_layout->addWidget(scroll);

  connect(refresh, &QPushButton::clicked, this,
          &RenderStatusPanel::RefreshStatus);
  connect(reset, &QPushButton::clicked, this, [this] {
    view_->ResetRendererStats();
    RefreshStatus();
    emit statusMessage(QStringLiteral("渲染计数已清零"));
  });
  // 文档重建（含主题/开关切换）都会更新计数，顺手刷新
  connect(view_, &MarkdownView::outlineChanged, this,
          &RenderStatusPanel::RefreshStatus);
  RefreshStatus();
}

void RenderStatusPanel::RefreshStatus() {
  const MarkdownRendererStatus status = view_->renderer_status();
  highlight_->setText(QStringLiteral("高亮：%1\n%2")
                          .arg(EngineStateText(status.highlighter),
                               CounterText(status.highlight_context)));
  formula_->setText(QStringLiteral("公式：%1\n%2")
                        .arg(EngineStateText(status.formula),
                             CounterText(status.formula_context)));
  mermaid_->setText(QStringLiteral("Mermaid：%1\n%2")
                        .arg(EngineStateText(status.mermaid),
                             CounterText(status.mermaid_context)));
}
