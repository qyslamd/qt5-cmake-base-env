#include "log_output_panel.h"

#include <QScrollBar>
#include <QTextCursor>

#include <spdlog/spdlog.h>

#include "logger/logger.h"
#include "logger/qt_console_sink.h"

LogOutputPanel::LogOutputPanel(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();

  // 实时日志：QtConsoleSink::logMessage -> appendLog（queued 跨线程）
  if (auto* sink = Logger::qtConsoleSink()) {
    connect(sink, &QtConsoleSink::logMessage, this,
            &LogOutputPanel::appendLog);
  }

  // 拉取 Logger::init() 之后到本构造之间的历史日志
  if (auto* history = Logger::qtHistoryBuffer()) {
    connect(history, &LogHistoryBuffer::drained, this,
            &LogOutputPanel::onHistoricalLogs);
    history->drain(this);
  }
}

void LogOutputPanel::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  filter_bar_ = new LogFilterBar(this);
  layout->addWidget(filter_bar_);

  text_edit_ = new QTextEdit(this);
  text_edit_->setReadOnly(true);
  text_edit_->setFrameShape(QFrame::NoFrame);
  text_edit_->setObjectName(QStringLiteral("logOutputView"));
  text_edit_->setPlaceholderText(QStringLiteral("输出信息将显示在此处..."));
  // 与 entries_ 同步裁剪，避免文档无限增长
  text_edit_->document()->setMaximumBlockCount(kMaxEntries);
  layout->addWidget(text_edit_);

  current_filter_ = filter_bar_->filter();
}

void LogOutputPanel::initSignals() {
  debounce_timer_ = new QTimer(this);
  debounce_timer_->setSingleShot(true);
  debounce_timer_->setInterval(300);
  connect(debounce_timer_, &QTimer::timeout, this,
          &LogOutputPanel::applyFilter);

  connect(filter_bar_, &LogFilterBar::filterChanged, this,
          &LogOutputPanel::onFilterChanged);
  connect(filter_bar_, &LogFilterBar::scrollLockChanged, this,
          &LogOutputPanel::onScrollLockChanged);
  connect(filter_bar_, &LogFilterBar::clearRequested, this,
          &LogOutputPanel::clearLog);
}

void LogOutputPanel::appendLog(int level, const QString& text) {
  entries_.append(LogEntry{level, text});
  while (entries_.size() > kMaxEntries) {
    entries_.removeFirst();
  }
  if (current_filter_.matches(level, text)) {
    renderEntry(entries_.last());
  }
}

void LogOutputPanel::clearLog() {
  entries_.clear();
  text_edit_->clear();
}

void LogOutputPanel::renderEntry(const LogEntry& entry) {
  QScrollBar* bar = text_edit_->verticalScrollBar();
  bool was_at_bottom =
      bar->maximum() > 0 && bar->value() >= bar->maximum() - 2;
  text_edit_->append(buildEntryHtml(entry));
  // 仅当原先在底部才自动滚动，用户上翻时不打断阅读
  if (!scroll_locked_ && was_at_bottom) {
    bar->setValue(bar->maximum());
  }
}

QString LogOutputPanel::buildEntryHtml(const LogEntry& entry) const {
  QString color = levelColor(entry.level);

  // 无文本过滤或正则非法 -> 整行着色，不高亮
  if (current_filter_.text.isEmpty() ||
      (current_filter_.use_regex && !current_filter_.isValid())) {
    return QStringLiteral("<span style=\"color:%1\">%2</span>")
        .arg(color, entry.text.toHtmlEscaped());
  }

  // 命中区间（基于原始 text 索引，与 matches 共用逻辑）
  QList<QPair<int, int>> ranges = current_filter_.matchRanges(entry.text);
  if (ranges.isEmpty()) {
    return QStringLiteral("<span style=\"color:%1\">%2</span>")
        .arg(color, entry.text.toHtmlEscaped());
  }

  // 按命中区间分段拼接：未命中段级别色，命中段黄底黑字高亮
  QString html;
  int cursor = 0;
  for (const auto& r : ranges) {
    if (r.first > cursor) {
      QString seg = entry.text.mid(cursor, r.first - cursor);
      html += QStringLiteral("<span style=\"color:%1\">%2</span>")
                  .arg(color, seg.toHtmlEscaped());
    }
    QString hit = entry.text.mid(r.first, r.second - r.first);
    html += QStringLiteral(
                "<span style=\"background:#FFD700;color:#000\">%1</span>")
                .arg(hit.toHtmlEscaped());
    cursor = r.second;
  }
  if (cursor < entry.text.size()) {
    QString seg = entry.text.mid(cursor);
    html += QStringLiteral("<span style=\"color:%1\">%2</span>")
                .arg(color, seg.toHtmlEscaped());
  }
  return html;
}

QString LogOutputPanel::levelColor(int level) const {
  switch (level) {
    case spdlog::level::debug:
      return QStringLiteral("#858585");
    case spdlog::level::info:
      return QStringLiteral("#333333");
    case spdlog::level::warn:
      return QStringLiteral("#D7BA7D");
    case spdlog::level::err:
      return QStringLiteral("#F44747");
    case spdlog::level::critical:
      return QStringLiteral("#FF0000");
    default:
      return QStringLiteral("#333333");
  }
}

void LogOutputPanel::applyFilter() {
  QScrollBar* bar = text_edit_->verticalScrollBar();
  int old_value = bar->value();
  int old_max = bar->maximum();
  bool was_at_bottom = old_max > 0 && old_value >= old_max - 2;

  text_edit_->clear();
  QTextCursor cursor(text_edit_->document());
  cursor.beginEditBlock();
  bool first = true;
  for (const auto& entry : entries_) {
    if (!current_filter_.matches(entry.level, entry.text)) {
      continue;
    }
    if (!first) {
      cursor.insertBlock();
    }
    cursor.insertHtml(buildEntryHtml(entry));
    first = false;
  }
  cursor.endEditBlock();

  if (was_at_bottom) {
    bar->setValue(bar->maximum());
  } else if (old_max > 0 && bar->maximum() > 0) {
    bar->setValue(static_cast<int>(bar->maximum() *
                                   (static_cast<double>(old_value) / old_max)));
  }
}

void LogOutputPanel::requestFilterApply() { debounce_timer_->start(); }

void LogOutputPanel::onHistoricalLogs(const QList<LogEntry>& entries) {
  for (const auto& entry : entries) {
    entries_.append(entry);
  }
  while (entries_.size() > kMaxEntries) {
    entries_.removeFirst();
  }
  applyFilter();
}

void LogOutputPanel::onFilterChanged(const LogFilter& filter) {
  current_filter_ = filter;
  if (filter.use_regex && !filter.isValid()) {
    filter_bar_->setFilterInvalid(true, filter.errorString());
  } else {
    filter_bar_->setFilterInvalid(false);
  }
  requestFilterApply();
}

void LogOutputPanel::onScrollLockChanged(bool locked) {
  scroll_locked_ = locked;
}
