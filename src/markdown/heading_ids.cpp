#include "heading_ids.h"

#include <QHash>
#include <QRegularExpression>

namespace {

// HTML 实体解码：md4c 只产出这几个，够用
QString DecodeEntities(const QString& text) {
  QString out = text;
  out.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
  out.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
  out.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
  out.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
  // & 放最后，免得把前面几种解出来的东西二次解码
  out.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
  return out;
}

QString Slugify(const QString& text) {
  QString out;
  for (const QChar c : text) {
    if (c.isLetterOrNumber()) {
      out += c.toLower();
    } else if (c.isSpace()) {
      out += QLatin1Char('-');
    } else if (c == QLatin1Char('-') || c == QLatin1Char('_')) {
      out += c;
    }
  }
  return out;
}

}  // namespace

QString AddHeadingIds(const QString& html, const HeadingSink& on_heading) {
  static const QRegularExpression kHeading(
      QStringLiteral("<h([1-6])>(.*?)</h\\1>"),
      QRegularExpression::DotMatchesEverythingOption);
  static const QRegularExpression kTag(QStringLiteral("<[^>]*>"));

  QString out;
  int pos = 0;
  int index = 0;  // 全局序号：重名 slug 靠它拿到彼此不同的 id
  QHash<QString, int> seen;
  auto it = kHeading.globalMatch(html);
  while (it.hasNext()) {
    const auto match = it.next();
    out += html.mid(pos, match.capturedStart() - pos);
    pos = match.capturedEnd();

    const QString level = match.captured(1);
    const QString inner = match.captured(2);

    // slug 用纯文本算：<h2>Hello <code>world</code></h2> 的锚点是
    // #hello-world，不把标签算进去
    QString plain = inner;
    plain.remove(kTag);
    plain = DecodeEntities(plain);

    QString slug = Slugify(plain);
    const int n = seen.value(slug, 0);
    seen[slug] = n + 1;
    if (n > 0) {
      slug += QStringLiteral("-%1").arg(n);
    }

    // id 必须是纯 ASCII：litehtml 解析时会改写 id 的存储形式，UTF-8 多字节
    // 经不起这一趟，非 ASCII 的 id 会让 scrollToAnchor 静默找不到元素。
    // 原 slug 存进 data-md-slug，宿主点击时拿它反查 id
    const QString ascii_id = QStringLiteral("h-%1").arg(index++);
    if (on_heading) {
      on_heading(slug, ascii_id);
    }

    out += QStringLiteral("<h%1 id=\"%2\" data-md-slug=\"%3\">%4</h%1>")
               .arg(level, ascii_id, slug.toHtmlEscaped(), inner);
  }
  out += html.mid(pos);
  return out;
}
