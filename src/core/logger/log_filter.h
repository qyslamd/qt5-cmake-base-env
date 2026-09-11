#ifndef FILE_CORE_LOGGER_LOG_FILTER_H_
#define FILE_CORE_LOGGER_LOG_FILTER_H_

#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <QSet>
#include <QString>

// 日志筛选条件（纯数据结构，与 UI 解耦）。
// enabledLevels 恒非空：LogFilterBar 拦截「取消最后一个勾选」保证。
struct LogFilter {
  QString text;  // 过滤文本（普通模式为字面量，正则模式为正则表达式）
  bool use_regex = false;
  bool case_sensitive = false;
  QSet<int> enabled_levels;  // 启用的级别集合

  // 单条日志是否命中：级别不在集合 -> false；text 空 -> true；
  // 普通模式字面量包含；正则模式合法则匹配，非法退化为「文本不限」。
  bool matches(int level, const QString& entry_text) const;

  // text 在 entry_text 中的命中区间 [start, end)；无命中返回空列表。
  QList<QPair<int, int>> matchRanges(const QString& entry_text) const;

  // 正则模式且 text 非空时返回正则是否合法；其余恒 true。
  bool isValid() const;

  // 正则非法时的错误描述；非正则/合法返回空串。
  QString errorString() const;

 private:
  QRegularExpression compileRegex() const;
};

#endif  // FILE_CORE_LOGGER_LOG_FILTER_H_
