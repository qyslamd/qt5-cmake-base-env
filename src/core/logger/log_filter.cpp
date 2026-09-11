#include "log_filter.h"

bool LogFilter::matches(int level, const QString& entry_text) const {
  if (!enabled_levels.contains(level)) {
    return false;
  }
  if (text.isEmpty()) {
    return true;
  }
  if (use_regex) {
    QRegularExpression re = compileRegex();
    if (!re.isValid()) {
      return true;  // 非法正则退化为「文本不限」
    }
    return re.match(entry_text).hasMatch();
  }
  Qt::CaseSensitivity cs =
      case_sensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
  return entry_text.contains(text, cs);
}

QList<QPair<int, int>> LogFilter::matchRanges(const QString& entry_text) const {
  QList<QPair<int, int>> ranges;
  if (text.isEmpty()) {
    return ranges;
  }
  if (use_regex) {
    QRegularExpression re = compileRegex();
    if (!re.isValid()) {
      return ranges;
    }
    auto it = re.globalMatch(entry_text);
    while (it.hasNext()) {
      QRegularExpressionMatch m = it.next();
      ranges.append({m.capturedStart(), m.capturedEnd()});
    }
    return ranges;
  }
  Qt::CaseSensitivity cs =
      case_sensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
  int from = 0;
  while (from < entry_text.size()) {
    int idx = entry_text.indexOf(text, from, cs);
    if (idx < 0) {
      break;
    }
    ranges.append({idx, idx + text.size()});
    from = idx + text.size();
  }
  return ranges;
}

bool LogFilter::isValid() const {
  if (!use_regex || text.isEmpty()) {
    return true;
  }
  return compileRegex().isValid();
}

QString LogFilter::errorString() const {
  if (!use_regex || text.isEmpty()) {
    return QString();
  }
  return compileRegex().errorString();
}

QRegularExpression LogFilter::compileRegex() const {
  QRegularExpression::PatternOptions options;
  if (!case_sensitive) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }
  return QRegularExpression(text, options);
}
