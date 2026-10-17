#include "render_settings_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "markdown_render_context.h"
#include "markdown_types.h"
#include "markdown_view.h"

namespace {

// Fluent Gallery 的卡片写法：isCard + WA_StyledBackground，样式由 style 画
QWidget* NewCard(QWidget* parent, const QString& title, QVBoxLayout** content) {
  auto* card = new QWidget(parent);
  card->setProperty("isCard", true);
  card->setAttribute(Qt::WA_StyledBackground, true);
  auto* layout = new QVBoxLayout(card);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(10);

  auto* label = new QLabel(title, card);
  QFont font = label->font();
  font.setBold(true);
  label->setFont(font);
  layout->addWidget(label);

  *content = layout;
  return card;
}

void AddSwitchRow(QVBoxLayout* layout, QWidget* parent, const QString& text,
                  QCheckBox* box, const QString& tooltip) {
  auto* label = new QLabel(text, parent);
  label->setToolTip(tooltip);
  box->setToolTip(tooltip);
  layout->addWidget(label);
  layout->addWidget(box);
}

// 只读路径 + 选择/默认两个按钮
QWidget* NewPathRow(QWidget* parent, QLineEdit* edit, QPushButton** choose,
                    QPushButton** reset, const QString& choose_text,
                    const QString& reset_text) {
  auto* row = new QWidget(parent);
  auto* layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  edit->setReadOnly(true);
  layout->addWidget(edit, 1);
  *choose = new QPushButton(choose_text, row);
  *reset = new QPushButton(reset_text, row);
  layout->addWidget(*choose);
  layout->addWidget(*reset);
  return row;
}

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

RenderSettingsPanel::RenderSettingsPanel(MarkdownView* view, QWidget* parent)
    : QWidget(parent), view_(view) {
  initUi();
  initSignals();
}

QCheckBox* RenderSettingsPanel::CreateSwitch(bool checked) {
  auto* box = new QCheckBox(
      checked ? QStringLiteral("开") : QStringLiteral("关"), this);
  box->setProperty("isSwitchButton", true);
  box->setChecked(checked);
  connect(box, &QCheckBox::toggled, box, [box](bool on) {
    box->setText(on ? QStringLiteral("开") : QStringLiteral("关"));
  });
  return box;
}

void RenderSettingsPanel::initUi() {
  // 面板放进滚动区：dock 高度可以随便压，内容不会被挤扁
  auto* panel_layout = new QVBoxLayout(this);
  panel_layout->setContentsMargins(0, 0, 0, 0);
  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto* body = new QWidget;
  auto* outer = new QVBoxLayout(body);
  outer->setContentsMargins(12, 12, 12, 12);
  outer->setSpacing(12);

  // 外观
  QVBoxLayout* appearance = nullptr;
  QWidget* appearance_card = NewCard(body, QStringLiteral("外观"), &appearance);
  theme_ = new QComboBox(appearance_card);
  theme_->addItem(QStringLiteral("浅色"),
                  static_cast<int>(MarkdownTheme::Light));
  theme_->addItem(QStringLiteral("暗色"),
                  static_cast<int>(MarkdownTheme::Dark));
  theme_->addItem(QStringLiteral("高对比"),
                  static_cast<int>(MarkdownTheme::HighContrast));
  const int theme_index = theme_->findData(static_cast<int>(view_->theme()));
  theme_->setCurrentIndex(theme_index >= 0 ? theme_index : 0);
  appearance->addWidget(new QLabel(QStringLiteral("主题"), appearance_card));
  appearance->addWidget(theme_);
  outer->addWidget(appearance_card);

  // 内容
  QVBoxLayout* content = nullptr;
  QWidget* content_card = NewCard(body, QStringLiteral("内容"), &content);
  code_line_numbers_ = CreateSwitch(view_->code_line_numbers());
  AddSwitchRow(content, content_card, QStringLiteral("代码块行号"),
               code_line_numbers_,
               QStringLiteral("切换后重渲染并保留滚动位置"));
  formula_rendering_ = CreateSwitch(view_->formula_rendering_enabled());
  AddSwitchRow(content, content_card, QStringLiteral("公式渲染"),
               formula_rendering_,
               QStringLiteral("关闭后 $…$ 原样显示，不加载 MathJax"));
  mermaid_rendering_ = CreateSwitch(view_->mermaid_rendering_enabled());
  AddSwitchRow(content, content_card, QStringLiteral("Mermaid 渲染"),
               mermaid_rendering_,
               QStringLiteral("关闭后 mermaid 围栏退回代码块"));
  outer->addWidget(content_card);

  // 持久缓存
  QVBoxLayout* cache = nullptr;
  QWidget* cache_card = NewCard(body, QStringLiteral("持久缓存"), &cache);
  persistent_cache_ = CreateSwitch(view_->persistent_render_cache_enabled());
  AddSwitchRow(cache, cache_card, QStringLiteral("启用磁盘缓存"),
               persistent_cache_,
               QStringLiteral("关闭只跳过磁盘读写，内存缓存照常"));
  cache_directory_ = new QLineEdit(cache_card);
  cache_directory_->setText(view_->render_context().options().cache_directory);
  cache_directory_->setPlaceholderText(QStringLiteral("默认位置"));
  cache->addWidget(NewPathRow(cache_card, cache_directory_, &choose_cache_,
                              &reset_cache_, QStringLiteral("选择…"),
                              QStringLiteral("默认")));
  auto* cache_actions = new QWidget(cache_card);
  auto* cache_actions_layout = new QHBoxLayout(cache_actions);
  cache_actions_layout->setContentsMargins(0, 0, 0, 0);
  cache_actions_layout->setSpacing(6);
  clear_memory_cache_ =
      new QPushButton(QStringLiteral("清空内存缓存"), cache_actions);
  clear_cache_ = new QPushButton(QStringLiteral("清除磁盘缓存"), cache_actions);
  cache_actions_layout->addWidget(clear_memory_cache_);
  cache_actions_layout->addWidget(clear_cache_);
  cache->addWidget(cache_actions);
  outer->addWidget(cache_card);

  // 诊断
  QVBoxLayout* diagnostics = nullptr;
  QWidget* diagnostics_card =
      NewCard(body, QStringLiteral("诊断"), &diagnostics);
  trace_ = CreateSwitch(view_->diagnostics().trace);
  AddSwitchRow(diagnostics, diagnostics_card, QStringLiteral("trace 日志"),
               trace_,
               QStringLiteral("输出 qt.markdown.render，日志量可能较大"));
  dump_directory_ = new QLineEdit(diagnostics_card);
  dump_directory_->setText(view_->diagnostics().dump_directory);
  dump_directory_->setPlaceholderText(QStringLiteral("未开启"));
  dump_directory_->setToolTip(
      QStringLiteral("中间产物可能包含文档正文，仅调试时开启"));
  diagnostics->addWidget(
      NewPathRow(diagnostics_card, dump_directory_, &choose_dump_, &reset_dump_,
                 QStringLiteral("选择…"), QStringLiteral("关闭")));
  outer->addWidget(diagnostics_card);

  // 渲染状态：引擎是否加载 + 各上下文的渲染/命中计数
  QVBoxLayout* status = nullptr;
  QWidget* status_card = NewCard(body, QStringLiteral("渲染状态"), &status);
  status_highlight_ = new QLabel(status_card);
  status_highlight_->setObjectName(QStringLiteral("statusHighlight"));
  status_formula_ = new QLabel(status_card);
  status_formula_->setObjectName(QStringLiteral("statusFormula"));
  status_mermaid_ = new QLabel(status_card);
  status_mermaid_->setObjectName(QStringLiteral("statusMermaid"));
  for (QLabel* label : {status_highlight_, status_formula_, status_mermaid_}) {
    label->setWordWrap(true);
    status->addWidget(label);
  }
  auto* status_actions = new QWidget(status_card);
  auto* status_actions_layout = new QHBoxLayout(status_actions);
  status_actions_layout->setContentsMargins(0, 0, 0, 0);
  status_actions_layout->setSpacing(6);
  refresh_status_ = new QPushButton(QStringLiteral("刷新"), status_actions);
  reset_stats_ = new QPushButton(QStringLiteral("计数清零"), status_actions);
  status_actions_layout->addWidget(refresh_status_);
  status_actions_layout->addWidget(reset_stats_);
  status_actions_layout->addStretch(1);
  status->addWidget(status_actions);
  outer->addWidget(status_card);

  outer->addStretch(1);
  scroll->setWidget(body);
  panel_layout->addWidget(scroll);
  RefreshStatus();
}

void RenderSettingsPanel::initSignals() {
  connect(theme_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int index) {
            view_->SetTheme(
                static_cast<MarkdownTheme>(theme_->itemData(index).toInt()));
          });
  connect(code_line_numbers_, &QCheckBox::toggled, view_,
          &MarkdownView::SetCodeLineNumbers);
  connect(formula_rendering_, &QCheckBox::toggled, view_,
          &MarkdownView::SetFormulaRenderingEnabled);
  connect(mermaid_rendering_, &QCheckBox::toggled, view_,
          &MarkdownView::SetMermaidRenderingEnabled);
  connect(persistent_cache_, &QCheckBox::toggled, view_,
          &MarkdownView::SetPersistentRenderCacheEnabled);
  connect(trace_, &QCheckBox::toggled, this, [this] { ApplyDiagnostics(); });
  connect(choose_cache_, &QPushButton::clicked, this,
          &RenderSettingsPanel::BrowseCacheDirectory);
  connect(reset_cache_, &QPushButton::clicked, this, [this] {
    cache_directory_->clear();
    view_->SetRenderCacheDirectory(QString());
    emit statusMessage(QStringLiteral("持久缓存目录恢复默认"));
  });
  connect(clear_memory_cache_, &QPushButton::clicked, this, [this] {
    view_->ClearRenderCaches();
    RefreshStatus();
    emit statusMessage(QStringLiteral("已清空内存缓存"));
  });
  connect(clear_cache_, &QPushButton::clicked, this, [this] {
    view_->ClearPersistentRenderCache();
    emit statusMessage(QStringLiteral("已清除持久渲染缓存"));
  });
  connect(refresh_status_, &QPushButton::clicked, this,
          &RenderSettingsPanel::RefreshStatus);
  connect(reset_stats_, &QPushButton::clicked, this, [this] {
    view_->ResetRendererStats();
    RefreshStatus();
    emit statusMessage(QStringLiteral("渲染计数已清零"));
  });
  // 每次文档重建（含主题/开关切换）都会更新计数，顺手刷新状态
  connect(view_, &MarkdownView::outlineChanged, this,
          &RenderSettingsPanel::RefreshStatus);
  connect(choose_dump_, &QPushButton::clicked, this,
          &RenderSettingsPanel::BrowseDumpDirectory);
  connect(reset_dump_, &QPushButton::clicked, this, [this] {
    dump_directory_->clear();
    ApplyDiagnostics();
  });
}

void RenderSettingsPanel::ApplyDiagnostics() {
  MarkdownDiagnostics diagnostics;
  diagnostics.trace = trace_->isChecked();
  diagnostics.dump_directory = dump_directory_->text().trimmed();
  view_->SetDiagnostics(diagnostics);
}

void RenderSettingsPanel::BrowseCacheDirectory() {
  const QString dir = QFileDialog::getExistingDirectory(
      this, QStringLiteral("选择持久缓存目录"),
      cache_directory_->text().isEmpty() ? QDir::homePath()
                                         : cache_directory_->text());
  if (dir.isEmpty()) {
    return;
  }
  cache_directory_->setText(dir);
  view_->SetRenderCacheDirectory(dir);
}

void RenderSettingsPanel::RefreshStatus() {
  const MarkdownRendererStatus status = view_->renderer_status();
  status_highlight_->setText(QStringLiteral("高亮：%1\n%2")
                                 .arg(EngineStateText(status.highlighter),
                                      CounterText(status.highlight_context)));
  status_formula_->setText(QStringLiteral("公式：%1\n%2")
                               .arg(EngineStateText(status.formula),
                                    CounterText(status.formula_context)));
  status_mermaid_->setText(QStringLiteral("Mermaid：%1\n%2")
                               .arg(EngineStateText(status.mermaid),
                                    CounterText(status.mermaid_context)));
}

void RenderSettingsPanel::BrowseDumpDirectory() {
  const QString dir = QFileDialog::getExistingDirectory(
      this, QStringLiteral("选择诊断转储目录"),
      dump_directory_->text().isEmpty() ? QDir::homePath()
                                        : dump_directory_->text());
  if (dir.isEmpty()) {
    return;
  }
  dump_directory_->setText(dir);
  ApplyDiagnostics();
}
