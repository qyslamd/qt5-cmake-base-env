#ifndef FILE_APP_LOG_FILTER_BAR_H_
#define FILE_APP_LOG_FILTER_BAR_H_

#include <QHash>
#include <QLineEdit>
#include <QList>
#include <QToolButton>
#include <QWidget>

#include "logger/log_filter.h"

// 日志筛选工具栏：[D][I][W][E][F] [过滤框] [.*][Aa][锁][清]
// 级别按钮拦截「取消最后一个勾选」，保证 enabled_levels 恒非空。
class LogFilterBar : public QWidget {
  Q_OBJECT
 public:
  explicit LogFilterBar(QWidget* parent = nullptr);

  LogFilter filter() const;
  bool scrollLocked() const { return scroll_locked_; }
  void setFilterInvalid(bool invalid, const QString& error = QString());

 signals:
  void filterChanged(const LogFilter& filter);
  void scrollLockChanged(bool locked);
  void clearRequested();

 private:
  void initUi();
  void initSignals();
  void onLevelToggled(int level, bool checked);

  struct LevelDef {
    QString name;
    int level;
    QString object_suffix;
    QString full_name;
  };
  QList<LevelDef> level_defs_;
  QHash<int, QToolButton*> level_buttons_;

  QLineEdit* filter_edit_ = nullptr;
  QToolButton* regex_btn_ = nullptr;
  QToolButton* case_btn_ = nullptr;
  QToolButton* lock_btn_ = nullptr;
  QToolButton* clear_btn_ = nullptr;

  bool scroll_locked_ = false;
  bool last_invalid_ = false;
  QString last_error_;
};

#endif  // FILE_APP_LOG_FILTER_BAR_H_
