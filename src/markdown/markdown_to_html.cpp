#include "markdown_to_html.h"

#include <QRegularExpression>
#include <QStringList>
#include <QUuid>
#include <QVector>

#include "code_highlighter.h"
#include "html_escape.h"

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
//
// 占位符带每次调用的随机盐：占位符要在整份成品 HTML 上全局换回，固定格式
// （如 @@mathjax-0@@）会被用户文本/代码块里的同形字面量撞上，被错换成公式
QString MathPlaceholder(const QString& salt, int index) {
  return QStringLiteral("@@mj-%1-%2@@").arg(salt).arg(index);
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
QString ExtractMathInLine(const QString& line, QVector<MathSegment>* segments,
                          const QString& salt) {
  // 占位符比原文短、换行不变：按原长留底，逐段 append 不会反复扩容
  QString out;
  out.reserve(line.size());
  int i = 0;
  const int size = line.size();
  while (i < size) {
    const QChar current = line.at(i);

    // 行内代码：整段照抄，里面的 $ 不动
    if (current == QLatin1Char('`')) {
      int run = 1;
      while (i + run < size && line.at(i + run) == QLatin1Char('`')) {
        ++run;
      }
      const int close = line.indexOf(QString(run, QLatin1Char('`')), i + run);
      if (close < 0) {  // 没有配对的收尾，按普通文本处理
        out.append(line.constData() + i, size - i);
        break;
      }
      out.append(line.constData() + i, close + run - i);
      i = close + run;
      continue;
    }

    if (current != QLatin1Char('$')) {
      out += current;
      ++i;
      continue;
    }

    // 开标记与闭标记用同一条转义规则：数前面连续反斜杠的奇偶。被转义的 $
    // 原样输出（前面的反斜杠已按普通字符输出），md4c 会渲染成字面 $
    if (IsEscapedAt(line, i)) {
      out += current;
      ++i;
      continue;
    }

    // $$…$$：独立公式
    if (i + 1 < size && line.at(i + 1) == QLatin1Char('$')) {
      // 跳过 \$$ 这种转义的位置：indexOf 直接取第一个命中会把公式
      // 截在 \$$ 上（$…$ 分支有 IsEscapedAt，这条曾经漏了）
      int close = -1;
      for (int scan = line.indexOf(QStringLiteral("$$"), i + 2); scan >= 0;
           scan = line.indexOf(QStringLiteral("$$"), scan + 1)) {
        if (!IsEscapedAt(line, scan)) {
          close = scan;
          break;
        }
      }
      if (close > i + 2) {
        segments->append({line.mid(i + 2, close - i - 2), true});
        out += MathPlaceholder(salt, segments->size() - 1);
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
      out += MathPlaceholder(salt, segments->size() - 1);
      i = close + 1;
      continue;
    }

    out += current;
    ++i;
  }
  return out;
}

// ATX 标题：1~6 个 #，后面是空白或行尾
bool IsAtxHeading(const QString& trimmed) {
  int hashes = 0;
  while (hashes < trimmed.size() && trimmed.at(hashes) == QLatin1Char('#')) {
    ++hashes;
  }
  if (hashes < 1 || hashes > 6) {
    return false;
  }
  return hashes == trimmed.size() || trimmed.at(hashes).isSpace();
}

// 分隔线：同一种标记（* - _）至少 3 个，中间可以有空白
bool IsThematicBreak(const QString& trimmed) {
  if (trimmed.isEmpty()) {
    return false;
  }
  const QChar marker = trimmed.at(0);
  if (marker != QLatin1Char('*') && marker != QLatin1Char('-') &&
      marker != QLatin1Char('_')) {
    return false;
  }
  int count = 0;
  for (const QChar c : trimmed) {
    if (c == marker) {
      ++count;
    } else if (!c.isSpace()) {
      return false;
    }
  }
  return count >= 3;
}

// Setext 下划线：整行只有 = 或 -。只在前一行是段落时才算块标记——
// 空行后的 "=====" 在 CommonMark 里只是一段普通正文
bool IsSetextUnderline(const QString& trimmed) {
  if (trimmed.isEmpty()) {
    return false;
  }
  const QChar marker = trimmed.at(0);
  if (marker != QLatin1Char('=') && marker != QLatin1Char('-')) {
    return false;
  }
  for (const QChar c : trimmed) {
    if (c != marker) {
      return false;
    }
  }
  return true;
}

// 该行是否以引用标记开头；是则把标记剥掉，剩下的内容写进 rest
bool StripBlockQuoteMarker(const QString& trimmed, QString* rest) {
  if (!trimmed.startsWith(QLatin1Char('>'))) {
    return false;
  }
  int i = 0;
  while (i < trimmed.size() && trimmed.at(i) == QLatin1Char('>')) {
    ++i;
    if (i < trimmed.size() && trimmed.at(i).isSpace()) {
      ++i;
    }
  }
  *rest = trimmed.mid(i);
  return true;
}

// 这一行是否结束了当前段落（后面的 4 空格行应进缩进代码块）。
// 列表项不算：CommonMark 的 lazy continuation 让 "- item" 后的缩进行
// 仍是列表内容。引用行只有"空引用"或引用内是完整块时才结束段落，
// "> quote" 后的缩进行同样是 lazy continuation
bool EndsParagraph(const QString& trimmed, bool paragraph_open) {
  if (IsAtxHeading(trimmed) || IsThematicBreak(trimmed)) {
    return true;
  }
  if (paragraph_open && IsSetextUnderline(trimmed)) {
    return true;
  }
  QString quote_rest;
  if (StripBlockQuoteMarker(trimmed, &quote_rest)) {
    const QString inner = quote_rest.trimmed();
    return inner.isEmpty() || IsAtxHeading(inner) || IsThematicBreak(inner) ||
           (paragraph_open && IsSetextUnderline(inner));
  }
  return false;
}

// 逐行抽取：围栏代码块整段跳过；单独成行的 $$ 会把中间的行合并成一段公式
QString ExtractMath(const QString& markdown, QVector<MathSegment>* segments,
                    const QString& salt) {
  const QStringList lines = markdown.split(QLatin1Char('\n'));
  QStringList result;
  result.reserve(lines.size());

  bool in_fence = false;
  QChar fence_open_char;
  int fence_open_len = 0;
  QString display_block;
  bool in_display_block = false;
  // 缩进代码块（CommonMark：4 空格或 tab，且不能打断段落——段落开着的时候
  // 缩进行只是续行）。近似口径：段落态按"上一非围栏/非代码行是否非空"维护，
  // 列表项内的深缩进场景不覆盖
  bool in_indent_code = false;
  bool paragraph_open = false;

  for (const QString& line : lines) {
    const QString trimmed = line.trimmed();
    const bool blank = trimmed.isEmpty();
    // 缩进的围栏行（4 空格起头的 ```）不是围栏：在缩进代码块里是内容，
    // 在围栏里也不构成闭合（CommonMark 的闭合围栏不允许 4 空格缩进）
    const bool indented = line.startsWith(QLatin1String("    ")) ||
                          line.startsWith(QLatin1Char('\t'));

    // 缩进代码块（CommonMark：4 空格或 tab，且不能打断段落——段落开着的
    // 时候缩进行只是续行）。近似口径：段落态按"上一非围栏/非代码行是否
    // 非空"维护，列表项内的深缩进场景不覆盖
    if (!in_display_block && !in_fence && !in_indent_code && indented &&
        !paragraph_open) {
      in_indent_code = true;
    }
    if (in_indent_code) {
      if (indented || blank) {
        result << line;
        paragraph_open = false;
        continue;
      }
      // 非缩进非空：代码块到此为止。这行本身可能是围栏（缩进代码块后跟
      // 围栏极常见），不能吞掉围栏判定——落下去继续走
      in_indent_code = false;
    }

    // 围栏行：3 个以上同字符（` 或 ~）打头。开围栏后面可以跟 info
    // string；闭合围栏要求同字符、长度不短于开启、且后面只剩空白
    // （CommonMark）——只比前 3 个字符的话，```` 开的块会被 ``` 提前
    // 收掉，后面的 $ 抽取全乱
    QChar fence_char;
    int fence_run = 0;
    if (!trimmed.isEmpty() && (trimmed.at(0) == QLatin1Char('`') ||
                               trimmed.at(0) == QLatin1Char('~'))) {
      fence_char = trimmed.at(0);
      while (fence_run < trimmed.size() &&
             trimmed.at(fence_run) == fence_char) {
        ++fence_run;
      }
    }
    if (!in_display_block && !indented && fence_run >= 3) {
      if (!in_fence) {
        in_fence = true;
        fence_open_char = fence_char;
        fence_open_len = fence_run;
      } else if (fence_char == fence_open_char && fence_run >= fence_open_len &&
                 trimmed.mid(fence_run).trimmed().isEmpty()) {
        in_fence = false;
      }
      result << line;
      paragraph_open = false;  // 围栏行不是段落内容
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
        result << MathPlaceholder(salt, segments->size() - 1);
        paragraph_open = false;
      } else {
        display_block += line + QLatin1Char('\n');
      }
      continue;
    }
    if (trimmed == QLatin1String("$$")) {
      in_display_block = true;
      display_block.clear();
      paragraph_open = false;
      continue;
    }

    result << ExtractMathInLine(line, segments, salt);
    // 标题/分隔线/空引用之后是完整块，不能再把下一行当段落续行——否则
    // 后面的缩进代码块会被跳过判定，块里的 $ 被误当公式抽走
    paragraph_open = !blank && !EndsParagraph(trimmed, paragraph_open);
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
                    const FormulaSink& formula, const QString& salt) {
  // 盐是十六进制串，对正则安全
  const QRegularExpression kSlot(QStringLiteral("@@mj-%1-(\\d+)@@").arg(salt));
  // 渲染结果通常比占位符长，按原文留底已经省掉绝大多数扩容
  QString result;
  result.reserve(html.size());
  int consumed = 0;
  auto matches = kSlot.globalMatch(html);
  while (matches.hasNext()) {
    const QRegularExpressionMatch match = matches.next();
    result.append(html.constData() + consumed,
                  match.capturedStart() - consumed);
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
  result.append(html.constData() + consumed, html.size() - consumed);
  return result;
}

void AppendHtml(const MD_CHAR* text, MD_SIZE size, void* userdata) {
  static_cast<QString*>(userdata)->append(
      QString::fromUtf8(text, static_cast<int>(size)));
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
                            const CodeBlockSink& code_blocks,
                            const MarkdownRenderOptions& options) {
  static const QRegularExpression kPreCode(
      QStringLiteral("^(<pre><code([^>]*)>)([\\s\\S]*?)(</code></pre>)"),
      QRegularExpression::MultilineOption);
  static const QRegularExpression kLanguage(
      QStringLiteral("class=\"language-([^\"]+)\""));

  // 代码块会被包上 header 整体变长：按原文留底，剩下的扩容只发生在块边界
  QString result;
  result.reserve(html.size());
  int consumed = 0;
  int block_index = 0;  // 每个代码块一个 id，与 copy: 链接里的编号对应
  auto matches = kPreCode.globalMatch(html);
  while (matches.hasNext()) {
    const QRegularExpressionMatch match = matches.next();
    result.append(html.constData() + consumed,
                  match.capturedStart() - consumed);
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
    const QString highlighted = options.highlight
                                    ? options.highlight(code, language)
                                    : HighlightCode(code, language);
    result += BuildCodeBlockFrame(highlighted, language, block_id);
  }
  result.append(html.constData() + consumed, html.size() - consumed);
  return result;
}

}  // namespace

QString MarkdownToHtml(const QString& markdown, const FormulaSink& formula,
                       const MermaidSink& mermaid,
                       const CodeBlockSink& code_blocks,
                       const HeadingSink& heading_ids,
                       const MarkdownRenderOptions& options) {
  // 公式在 md4c 之前抽出占位符、之后换回渲染结果；没给回调就整段跳过，
  // 一个字符都不动（$…$ 于是原样显示）
  QVector<MathSegment> segments;
  // 占位符盐：一次渲染一个，抽与还必须同盐（见 MathPlaceholder 的注释）
  const QString math_salt =
      QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
  const QString source =
      formula ? ExtractMath(markdown, &segments, math_salt) : markdown;

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
  // 补标题锚点放在最后：前面的高亮/公式回填只会替换标签内部，不动 h1–h6
  // 的外框，这里的 id 因此不会被谁冲掉
  return AddHeadingIds(
      RestoreMath(HighlightCodeBlocks(html, mermaid, code_blocks, options),
                  segments, formula, math_salt),
      heading_ids, options.outline);
}
