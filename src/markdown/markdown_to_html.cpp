#include "markdown_to_html.h"

#include <QRegularExpression>
#include <QStringList>
#include <QVector>

#include "code_highlighter.h"

extern "C" {
#include "md4c-html.h"
}

namespace {

// 一段公式：tex 原文 + 是否独立成行
struct MathSegment {
  QString tex;
  bool display = false;
};

// 公式在 md4c 眼里就是个普通文本占位符。抽出来在两头处理，是因为 $ 在两处
// 意思不同：代码块/行内代码里是字面量，正文里才是公式——只有源码阶段分得清，
// 对成品 HTML 正则会把 `awk '{ print $1 }'` 也当公式
QString MathPlaceholder(int index) {
  return QStringLiteral("@@mathjax-%1@@").arg(index);
}

// 这个位置上的 $ 是不是被反斜杠转义了。数前面连续的反斜杠：奇数个才算转义
// （\\$ 是"转义的反斜杠 + 定界符"）。找闭标记时也要用它——只看主循环当前位置
// 会漏掉"闭标记落在 \$ 上"的情形（实测 bug：$5 到 $10 之间，转义 \$y\$ 被判成
// 一整段公式）
bool IsEscapedAt(const QString& line, int index) {
  int backslashes = 0;
  for (int i = index - 1; i >= 0 && line.at(i) == QLatin1Char('\\'); --i) {
    ++backslashes;
  }
  return backslashes % 2 == 1;
}

// 处理一行：跳过行内代码与转义的 \$，剩下的 $…$ / $$…$$ 换成占位符
QString ExtractMathInLine(const QString& line, QVector<MathSegment>* segments) {
  QString out;
  int i = 0;
  const int size = line.size();
  while (i < size) {
    const QChar current = line.at(i);

    // 转义的 \$：原样留着（md4c 会渲染成 $）
    if (current == QLatin1Char('\\') && i + 1 < size &&
        line.at(i + 1) == QLatin1Char('$')) {
      out += line.mid(i, 2);
      i += 2;
      continue;
    }

    // 行内代码：整段照抄，里面的 $ 不动
    if (current == QLatin1Char('`')) {
      int run = 1;
      while (i + run < size && line.at(i + run) == QLatin1Char('`')) {
        ++run;
      }
      const int close = line.indexOf(QString(run, QLatin1Char('`')), i + run);
      if (close < 0) {  // 没有配对的收尾，按普通文本处理
        out += line.mid(i);
        break;
      }
      out += line.mid(i, close + run - i);
      i = close + run;
      continue;
    }

    if (current != QLatin1Char('$')) {
      out += current;
      ++i;
      continue;
    }

    // $$…$$：独立公式
    if (i + 1 < size && line.at(i + 1) == QLatin1Char('$')) {
      const int close = line.indexOf(QStringLiteral("$$"), i + 2);
      if (close > i + 2) {
        segments->append({line.mid(i + 2, close - i - 2), true});
        out += MathPlaceholder(segments->size() - 1);
        i = close + 2;
        continue;
      }
      out += QStringLiteral("$$");
      i += 2;
      continue;
    }

    // $…$：行内公式。开标记后不能是空白，闭标记前不能是空白、后面不能跟数字
    // ——这两条是为了不和"价格在 $5 到 $10 之间"这类正文打架
    int close = -1;
    if (i + 1 < size && !line.at(i + 1).isSpace()) {
      for (int j = i + 1; j < size; ++j) {
        if (line.at(j) != QLatin1Char('$') || line.at(j - 1).isSpace() ||
            IsEscapedAt(line, j)) {
          continue;
        }
        if (j + 1 < size && line.at(j + 1).isDigit()) {
          break;  // "$5 … $10"：不配对，放弃
        }
        close = j;
        break;
      }
    }
    if (close > i + 1) {
      segments->append({line.mid(i + 1, close - i - 1), false});
      out += MathPlaceholder(segments->size() - 1);
      i = close + 1;
      continue;
    }

    out += current;
    ++i;
  }
  return out;
}

// 逐行抽取：围栏代码块整段跳过；单独成行的 $$ 会把中间的行合并成一段公式
QString ExtractMath(const QString& markdown, QVector<MathSegment>* segments) {
  const QStringList lines = markdown.split(QLatin1Char('\n'));
  QStringList result;
  result.reserve(lines.size());

  bool in_fence = false;
  QString fence_marker;
  QString display_block;
  bool in_display_block = false;

  for (const QString& line : lines) {
    const QString trimmed = line.trimmed();

    if (!in_display_block && (trimmed.startsWith(QLatin1String("```")) ||
                              trimmed.startsWith(QLatin1String("~~~")))) {
      const QString marker = trimmed.left(3);
      if (!in_fence) {
        in_fence = true;
        fence_marker = marker;
      } else if (marker == fence_marker) {
        in_fence = false;
      }
      result << line;
      continue;
    }
    if (in_fence) {
      result << line;
      continue;
    }

    if (in_display_block) {
      if (trimmed == QLatin1String("$$")) {
        segments->append({display_block, true});
        in_display_block = false;
        display_block.clear();
        result << MathPlaceholder(segments->size() - 1);
      } else {
        display_block += line + QLatin1Char('\n');
      }
      continue;
    }
    if (trimmed == QLatin1String("$$")) {
      in_display_block = true;
      display_block.clear();
      continue;
    }

    result << ExtractMathInLine(line, segments);
  }

  // 没闭合的块级公式：按原文吐回去，不当公式
  if (in_display_block) {
    result << QStringLiteral("$$");
    result << display_block.trimmed();
    result << QStringLiteral("$$");
  }
  return result.join(QLatin1Char('\n'));
}

// 占位符换回渲染结果。渲染不出来（或调用方没给回调）就显示原文——一条坏公式
// 不该吞掉整页
QString RestoreMath(const QString& html, const QVector<MathSegment>& segments,
                    const FormulaSink& formula) {
  static const QRegularExpression kSlot(QStringLiteral("@@mathjax-(\\d+)@@"));
  QString result;
  int consumed = 0;
  auto matches = kSlot.globalMatch(html);
  while (matches.hasNext()) {
    const QRegularExpressionMatch match = matches.next();
    result += html.mid(consumed, match.capturedStart() - consumed);
    consumed = match.capturedEnd();

    const int index = match.captured(1).toInt();
    if (index < 0 || index >= segments.size()) {
      continue;
    }
    const MathSegment& segment = segments.at(index);
    const QString rendered =
        formula ? formula(segment.tex, segment.display) : QString();
    if (!rendered.isEmpty()) {
      result += rendered;
      continue;
    }
    const QString source =
        segment.display
            ? QStringLiteral("$$") + segment.tex + QStringLiteral("$$")
            : QStringLiteral("$") + segment.tex + QStringLiteral("$");
    result += QStringLiteral("<code class=\"math-src\">%1</code>")
                  .arg(source.toHtmlEscaped());
  }
  result += html.mid(consumed);
  return result;
}

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
  // md4c 在 GFM 下一般发 &#39;，补 &apos; 是完整性（同一批实体别漏一个）
  out.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
  out.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
  return out;
}

// 把 <pre><code class="language-xxx">…</code></pre> 里的代码交给高亮器。
//
// 两个约束都是为了躲开"原始 HTML 直通"：md4c 在 GFM 下放行内联原始 HTML，
// 正文里若出现没闭合的裸 <pre><code>，不加行首锚点就会从那处一直匹配到文档
// 后面下一个 </code></pre>，把中间整段正文吞成"代码"。md4c 生成的块级代码块
// 永远从行首开始，锚 ^ 之后只认真块（正则用 Multiline 让 ^ 匹配每行行首）
// 代码块包一层带 header 的框：左边语言标签、右边"复制"链接。复制链接是
// copy:<id>，id 交给调用方去查原文（见 CodeBlockSink）——这里只负责把 id 写
// 进 HTML，不碰剪贴板（渲染层不该知道宿主用不用 Qt 剪贴板）
QString BuildCodeBlockFrame(const QString& highlighted, const QString& language,
                            const QString& block_id) {
  const QString lang_text =
      language.isEmpty() ? QStringLiteral("text") : language;
  QString out = QStringLiteral("<div class=\"code-wrap\">");
  out += QStringLiteral("<div class=\"code-header\">");
  out += QStringLiteral("<span class=\"code-lang\">%1</span>")
             .arg(lang_text.toHtmlEscaped());
  out += QStringLiteral("<a class=\"copy-btn\" href=\"copy:%1\">复制</a>")
             .arg(block_id);
  out += QStringLiteral("</div>");
  // 无语言的围栏不写 language- 类：那是"代码块声明了哪种语言"的契约，
  // header 上显示 text 只是给人看的
  if (language.isEmpty()) {
    out += QStringLiteral("<pre><code>%1</code></pre>").arg(highlighted);
  } else {
    out += QStringLiteral("<pre><code class=\"language-%1\">%2</code></pre>")
               .arg(language.toHtmlEscaped(), highlighted);
  }
  out += QStringLiteral("</div>");
  return out;
}

QString HighlightCodeBlocks(const QString& html, const MermaidSink& mermaid,
                            const CodeBlockSink& code_blocks) {
  static const QRegularExpression kPreCode(
      QStringLiteral("^(<pre><code([^>]*)>)([\\s\\S]*?)(</code></pre>)"),
      QRegularExpression::MultilineOption);
  static const QRegularExpression kLanguage(
      QStringLiteral("class=\"language-([^\"]+)\""));

  QString result;
  int consumed = 0;
  int block_index = 0;  // 每个代码块一个 id，与 copy: 链接里的编号对应
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

    // mermaid 围栏不出代码块，交给 sink 出图（sink 拿的是实体已还原的源码）；
    // 渲染不出来再退回代码块，不留白
    if (mermaid && language == QLatin1String("mermaid")) {
      const QString image = mermaid(code);
      if (!image.isEmpty()) {
        result +=
            QStringLiteral("<div class=\"mermaid-wrap\">%1</div>").arg(image);
        continue;
      }
    }

    const QString block_id = QString::number(block_index++);
    if (code_blocks) {
      code_blocks(block_id, code);
    }
    result +=
        BuildCodeBlockFrame(HighlightCode(code, language), language, block_id);
  }
  result += html.mid(consumed);
  return result;
}

}  // namespace

QString MarkdownToHtml(const QString& markdown, const FormulaSink& formula,
                       const MermaidSink& mermaid,
                       const CodeBlockSink& code_blocks) {
  // 公式在 md4c 之前抽出占位符、之后换回渲染结果；没给回调就整段跳过，
  // 一个字符都不动（$…$ 于是原样显示）
  QVector<MathSegment> segments;
  const QString source = formula ? ExtractMath(markdown, &segments) : markdown;

  const QByteArray utf8 = source.toUtf8();
  QString html;
  // SKIP_UTF8_BOM：本模块的调用方给的是已解码的 QString（BOM 早被 Qt 吃掉），
  // 但直接喂原始字节的调用方会因 BOM 把首个块解析成正文，一个 flag 免掉这个坑
  const int result =
      md_html(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), AppendHtml,
              &html, MD_DIALECT_GITHUB, MD_HTML_FLAG_SKIP_UTF8_BOM);
  if (result != 0) {
    return QStringLiteral("<p class=\"error\">Markdown 解析失败</p>");
  }
  return RestoreMath(HighlightCodeBlocks(html, mermaid, code_blocks), segments,
                     formula);
}
