#include "heading_ids.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>

#include "html_escape.h"

namespace {

QString Slugify(const QString& text) {
  QString out;
  out.reserve(text.size());
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

  // 标题标记只会让输出更长：先按原长留底，避免逐段 append 反复扩容
  QString out;
  out.reserve(html.size());
  int pos = 0;
  int index = 0;  // 全局序号：重名 slug 靠它拿到彼此不同的 id
  // 已分配给任何标题的 slug。只按"原始 slug 出现次数"加后缀会和显式
  // 同名的标题撞车：Foo / Foo-1 / Foo 的第三个会生成 foo-1
  QSet<QString> used;
  auto it = kHeading.globalMatch(html);
  while (it.hasNext()) {
    const auto match = it.next();
    out.append(html.constData() + pos, match.capturedStart() - pos);
    pos = match.capturedEnd();

    const QString level = match.captured(1);
    const QString inner = match.captured(2);

    // slug 用纯文本算：<h2>Hello <code>world</code></h2> 的锚点是
    // #hello-world，不把标签算进去
    QString plain = inner;
    plain.remove(kTag);
    plain = UnescapeHtml(plain);

    QString slug = Slugify(plain);
    const QString base = slug;
    int n = 0;
    while (used.contains(slug)) {
      ++n;
      slug = base + QStringLiteral("-%1").arg(n);
    }
    used.insert(slug);

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
  out.append(html.constData() + pos, html.size() - pos);
  return out;
}
