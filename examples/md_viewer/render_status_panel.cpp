#include "render_status_panel.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(10);

  auto* title = new QLabel(QStringLiteral("渲染状态"), this);
  QFont font = title->font();
  font.setBold(true);
  title->setFont(font);
  layout->addWidget(title);

  highlight_ = new QLabel(this);
  highlight_->setObjectName(QStringLiteral("statusHighlight"));
  formula_ = new QLabel(this);
  formula_->setObjectName(QStringLiteral("statusFormula"));
  mermaid_ = new QLabel(this);
  mermaid_->setObjectName(QStringLiteral("statusMermaid"));
  for (QLabel* label : {highlight_, formula_, mermaid_}) {
    label->setWordWrap(true);
    layout->addWidget(label);
  }

  auto* actions = new QWidget(this);
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
