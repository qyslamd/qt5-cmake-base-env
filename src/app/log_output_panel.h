#ifndef FILE_APP_LOG_OUTPUT_PANEL_H_
#define FILE_APP_LOG_OUTPUT_PANEL_H_

#include <QList>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "log_filter_bar.h"
#include "logger/log_filter.h"
#include "logger/log_history_buffer.h"

// 应用日志输出面板：数据流 QtConsoleSink::logMessage -> appendLog ->
// entries_ 缓存 -> 按筛选渲染。entries_ 为唯一数据源（5000 环形）。
class LogOutputPanel : public QWidget {
  Q_OBJECT
 public:
  explicit LogOutputPanel(QWidget* parent = nullptr);

  // 实时日志入口（MainWindow connect QtConsoleSink::logMessage）
  void appendLog(int level, const QString& text);
  void clearLog();

 private:
  void initUi();
  void initSignals();
  void renderEntry(const LogEntry& entry);
  QString buildEntryHtml(const LogEntry& entry) const;
  QString levelColor(int level) const;
  void applyFilter();
  void requestFilterApply();

 private slots:
  void onHistoricalLogs(const QList<LogEntry>& entries);
  void onFilterChanged(const LogFilter& filter);
  void onScrollLockChanged(bool locked);

 private:
  LogFilterBar* filter_bar_ = nullptr;
  QTextEdit* text_edit_ = nullptr;

  QList<LogEntry> entries_;
  LogFilter current_filter_;
  static constexpr int kMaxEntries = 5000;

  QTimer* debounce_timer_ = nullptr;
  bool scroll_locked_ = false;
};

#endif  // FILE_APP_LOG_OUTPUT_PANEL_H_
