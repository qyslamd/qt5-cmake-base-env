#include "html_escape.h"

QString EscapeHtml(const QString& text) {
  QString out = text;
  out.replace(QChar('&'), QStringLiteral("&amp;"));
  out.replace(QChar('<'), QStringLiteral("&lt;"));
  out.replace(QChar('>'), QStringLiteral("&gt;"));
  out.replace(QChar('"'), QStringLiteral("&quot;"));
  return out;
}

QString UnescapeHtml(const QString& text) {
  QString out = text;
  out.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
  out.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
  out.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
  out.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
  out.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
  // &amp; 必须最后：先解它会把 "&amp;lt;" 变成 "&lt;"，再被上一条二次解码
  out.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
  return out;
}
