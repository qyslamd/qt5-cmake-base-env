#include "markdown_to_html.h"

#include <QRegularExpression>

#include "code_highlighter.h"

extern "C" {
#include "md4c-html.h"
}

namespace {

void AppendHtml(const MD_CHAR* text, MD_SIZE size, void* userdata) {
  static_cast<QString*>(userdata)->append(
      QString::fromUtf8(text, static_cast<int>(size)));
}

// 还原 md4c 在代码块里做的实体转义，把原文交回给高亮器（它自己会重新转义）。
// &amp; 放最后：先还原它会把 "&amp;lt;" 变成 "&lt;"，再被下一条当作原文
QString UnescapeHtml(const QString& text) {
  QString out = text;
  out.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
  out.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
  out.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
  out.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
  return out;
}

// 把 <pre><code class="language-xxx">…</code></pre> 里的代码交给高亮器。
//
// 两个约束都是为了躲开"原始 HTML 直通"：md4c 在 GFM 下放行内联原始 HTML，
// 正文里若出现没闭合的裸 <pre><code>，不加行首锚点就会从那处一直匹配到文档
// 后面下一个 </code></pre>，把中间整段正文吞成"代码"。md4c 生成的块级代码块
// 永远从行首开始，锚 ^ 之后只认真块（正则用 Multiline 让 ^ 匹配每行行首）
QString HighlightCodeBlocks(const QString& html) {
  static const QRegularExpression kPreCode(
      QStringLiteral("^(<pre><code([^>]*)>)([\\s\\S]*?)(</code></pre>)"),
      QRegularExpression::MultilineOption);
  static const QRegularExpression kLanguage(
      QStringLiteral("class=\"language-([^\"]+)\""));

  QString result;
  int consumed = 0;
  auto matches = kPreCode.globalMatch(html);
  while (matches.hasNext()) {
    const QRegularExpressionMatch match = matches.next();
    result += html.mid(consumed, match.capturedStart() - consumed);
    consumed = match.capturedEnd();

    QString language;
    const QRegularExpressionMatch found = kLanguage.match(match.captured(2));
    if (found.hasMatch()) {
      language = found.captured(1);
    }

    // 代码里的 "<" 从 md4c 出来是实体，正文里不会出现字面的 "</code></pre>"，
    // 非贪婪匹配到这里就是代码结束
    const QString code = UnescapeHtml(match.captured(3));
    const QString highlighted = HighlightCode(code, language);
    if (language.isEmpty()) {
      result += QStringLiteral("<pre><code>%1</code></pre>").arg(highlighted);
    } else {
      result +=
          QStringLiteral("<pre><code class=\"language-%1\">%2</code></pre>")
              .arg(language, highlighted);
    }
  }
  result += html.mid(consumed);
  return result;
}

}  // namespace

QString MarkdownToHtml(const QString& markdown) {
  const QByteArray utf8 = markdown.toUtf8();
  QString html;
  // SKIP_UTF8_BOM：本模块的调用方给的是已解码的 QString（BOM 早被 Qt 吃掉），
  // 但直接喂原始字节的调用方会因 BOM 把首个块解析成正文，一个 flag 免掉这个坑
  const int result =
      md_html(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), AppendHtml,
              &html, MD_DIALECT_GITHUB, MD_HTML_FLAG_SKIP_UTF8_BOM);
  if (result != 0) {
    return QStringLiteral("<p class=\"error\">Markdown 解析失败</p>");
  }
  return HighlightCodeBlocks(html);
}
