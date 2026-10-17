#include "mermaid_css_inliner.h"

#include <algorithm>

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QVector>

namespace {

// 支持的选择器只有 mermaid 实际用到的那几类：#id、.class、tag、tag.class，
// 以及后代组合与子选择器 >。伪类、属性选择器、@ 规则一律放弃。
struct SimpleSelector {
  QString id;
  QString tag;
  QStringList classes;
};

struct SelectorPart {
  SimpleSelector sel;
  bool direct_child = false;  // 与前一部分之间有 '>'
};

struct ElementInfo {
  QString tag;
  QString id;
  QStringList classes;
};

struct CssRule {
  QVector<SelectorPart> chain;  // 从祖先到后代
  // specificity 三元组 (id, class, tag)，跨整条链累加——同属性按它决胜，
  // 而不是文档顺序
  int spec_ids = 0;
  int spec_classes = 0;
  int spec_tags = 0;
  QHash<QString, QString> decls;
  QSet<QString> important;
};

QString DecodeHtmlEntities(const QString& s) {
  QString r = s;
  r.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
  r.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
  r.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
  r.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
  r.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
  return r;
}

QString EncodeForXmlAttr(const QString& s) {
  QString r = s;
  r.replace(QChar('&'), QStringLiteral("&amp;"));
  r.replace(QChar('<'), QStringLiteral("&lt;"));
  r.replace(QChar('>'), QStringLiteral("&gt;"));
  r.replace(QChar('"'), QStringLiteral("&quot;"));
  return r;
}

bool ParseSimple(const QString& part, SimpleSelector* out) {
  if (part.isEmpty()) {
    return false;
  }
  if (part.contains(QChar(':')) || part.contains(QChar('[')) ||
      part.contains(QChar('+')) || part.contains(QChar('~')) ||
      part.contains(QChar('*'))) {
    return false;
  }

  int i = 0;
  const int n = part.size();
  while (i < n && (part[i].isLetterOrNumber() || part[i] == QChar('-'))) {
    ++i;
  }
  if (i > 0) {
    out->tag = part.left(i);
  }

  while (i < n) {
    const QChar c = part[i];
    if (c == QChar('#')) {
      ++i;
      const int start = i;
      while (i < n && (part[i].isLetterOrNumber() || part[i] == QChar('-') ||
                       part[i] == QChar('_'))) {
        ++i;
      }
      out->id = part.mid(start, i - start);
    } else if (c == QChar('.')) {
      ++i;
      const int start = i;
      while (i < n && (part[i].isLetterOrNumber() || part[i] == QChar('-') ||
                       part[i] == QChar('_'))) {
        ++i;
      }
      out->classes.append(part.mid(start, i - start));
    } else {
      return false;
    }
  }
  return true;
}

void ParseDecls(const QString& body, QHash<QString, QString>* decls,
                QSet<QString>* important) {
  // 不传 SkipEmptyParts：Qt::SkipEmptyParts 要 5.14，QString::SkipEmptyParts
  // 在 5.15 又弃用（同 code_highlighter.cpp 的口径），空项在这里跳过
  // TODO: 这里按 ';' 硬切，CSS 值里带引号的分号（合法但罕见，如
  // content:"a;b"）会被切碎。mermaid 现在的输出没这种写法，真遇到再补一个
  // 带引号状态的扫描
  const QStringList parts = body.split(QChar(';'));
  for (const QString& p : parts) {
    const int colon = p.indexOf(QChar(':'));
    if (colon < 0) {
      continue;
    }
    const QString prop = p.left(colon).trimmed();
    QString value = p.mid(colon + 1).trimmed();
    if (prop.isEmpty() || value.isEmpty()) {
      continue;
    }
    bool is_important = false;
    if (value.endsWith(QStringLiteral("!important"), Qt::CaseInsensitive)) {
      value.chop(10);
      value = value.trimmed();
      is_important = true;
    }
    (*decls)[prop] = value;
    if (is_important) {
      important->insert(prop);
    }
  }
}

// 选择器规范化：'text.actor>tspan' → 按空白切分时 '>' 要能独立成 token
QVector<CssRule> ParseCssRules(const QString& css) {
  QVector<CssRule> rules;
  int i = 0;
  const int n = css.size();
  while (i < n) {
    while (i < n && css[i].isSpace()) {
      ++i;
    }
    if (i >= n) {
      break;
    }

    const int brace = css.indexOf(QChar('{'), i);
    if (brace < 0) {
      break;
    }
    QString selector_block = css.mid(i, brace - i).trimmed();

    int j = brace + 1;
    int depth = 1;
    while (j < n && depth > 0) {
      if (css[j] == QChar('{')) {
        ++depth;
      } else if (css[j] == QChar('}')) {
        --depth;
      }
      ++j;
    }
    if (depth != 0) {
      break;
    }
    const QString body = css.mid(brace + 1, j - brace - 2);
    i = j;

    if (selector_block.startsWith(QChar('@'))) {
      continue;
    }

    selector_block.replace(QRegularExpression(QStringLiteral("\\s*>\\s*")),
                           QStringLiteral(" > "));
    selector_block.replace(QRegularExpression(QStringLiteral(">")),
                           QStringLiteral(" > "));

    const QStringList selectors = selector_block.split(QChar(','));
    for (const QString& sel : selectors) {
      const QString s = sel.trimmed();
      if (s.isEmpty()) {
        continue;
      }

      CssRule rule;
      bool ok = true;
      bool pending_child = false;
      const QStringList parts =
          s.split(QRegularExpression(QStringLiteral("\\s+")));
      for (const QString& part : parts) {
        if (part.isEmpty()) {
          continue;
        }
        if (part == QLatin1String(">")) {
          if (rule.chain.isEmpty()) {
            ok = false;
            break;
          }
          pending_child = true;
          continue;
        }
        SimpleSelector simple;
        if (!ParseSimple(part, &simple)) {
          ok = false;
          break;
        }
        SelectorPart sp;
        sp.sel = simple;
        sp.direct_child = pending_child && !rule.chain.isEmpty();
        pending_child = false;
        rule.chain.append(sp);
        rule.spec_ids += simple.id.isEmpty() ? 0 : 1;
        rule.spec_classes += simple.classes.size();
        rule.spec_tags += simple.tag.isEmpty() ? 0 : 1;
      }
      if (!ok || rule.chain.isEmpty() || pending_child) {
        continue;
      }
      ParseDecls(body, &rule.decls, &rule.important);
      rules.append(rule);
    }
  }
  return rules;
}

bool MatchSimple(const ElementInfo& e, const SimpleSelector& s) {
  if (!s.id.isEmpty() && e.id != s.id) {
    return false;
  }
  if (!s.tag.isEmpty() && e.tag != s.tag) {
    return false;
  }
  for (const QString& c : s.classes) {
    if (!e.classes.contains(c)) {
      return false;
    }
  }
  return true;
}

// 从右往左匹配整条链：只匹配最后一段会漏掉 "#m0 .node rect" 这类规则
bool MatchChain(const QVector<ElementInfo>& stack, const CssRule& rule) {
  if (rule.chain.isEmpty() || stack.isEmpty()) {
    return false;
  }
  int si = stack.size() - 1;
  int ci = rule.chain.size() - 1;
  if (!MatchSimple(stack[si], rule.chain[ci].sel)) {
    return false;
  }
  --si;
  --ci;
  while (ci >= 0) {
    if (rule.chain[ci + 1].direct_child) {
      if (si < 0 || !MatchSimple(stack[si], rule.chain[ci].sel)) {
        return false;
      }
      --si;
      --ci;
    } else {
      bool found = false;
      while (si >= 0) {
        if (MatchSimple(stack[si], rule.chain[ci].sel)) {
          found = true;
          --si;
          --ci;
          break;
        }
        --si;
      }
      if (!found) {
        return false;
      }
    }
  }
  return true;
}

QString BuildStyle(const QHash<QString, QString>& decls,
                   const QSet<QString>& important) {
  QString out;
  for (auto it = decls.constBegin(); it != decls.constEnd(); ++it) {
    if (!out.isEmpty()) {
      out += QChar(';');
    }
    out += it.key();
    out += QChar(':');
    out += it.value();
    if (important.contains(it.key())) {
      out += QStringLiteral(" !important");
    }
  }
  return out;
}

}  // namespace

QString InlineSvgCss(const QString& svg, const QString& font_family) {
  static const QRegularExpression kStyle(
      QStringLiteral("<style[^>]*>(.*?)</style>"),
      QRegularExpression::DotMatchesEverythingOption);

  QString css;
  QString svg_without_style;
  int pos = 0;
  auto it = kStyle.globalMatch(svg);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    css += match.captured(1) + QChar('\n');
    svg_without_style += svg.mid(pos, match.capturedStart() - pos);
    pos = match.capturedEnd();
  }
  svg_without_style += svg.mid(pos);

  // 实体先解码再解析：font-family:&quot;trebuchet ms&quot; 里的那个 ';' 是实体
  // 的一部分，按 CSS 语法切分会把值截成 &quot，属性损坏、整份 SVG 解析失败
  css = DecodeHtmlEntities(css);

  const QVector<CssRule> parsed = ParseCssRules(css);
  if (parsed.isEmpty()) {
    return svg_without_style;
  }
  // 同属性按 specificity 决胜、同 specificity 按文档顺序：排序一次，
  // 下面的应用循环顺序本身就成了优先级
  QVector<CssRule> rules = parsed;
  std::stable_sort(rules.begin(), rules.end(),
                   [](const CssRule& a, const CssRule& b) {
                     if (a.spec_ids != b.spec_ids) {
                       return a.spec_ids < b.spec_ids;
                     }
                     if (a.spec_classes != b.spec_classes) {
                       return a.spec_classes < b.spec_classes;
                     }
                     return a.spec_tags < b.spec_tags;
                   });

  static const QRegularExpression kTag(
      QStringLiteral("<(/)?([a-zA-Z][a-zA-Z0-9:_-]*)((?:\\s[^>]*?)?)(/)?>"),
      QRegularExpression::DotMatchesEverythingOption);
  static const QRegularExpression kAttr(
      QStringLiteral("([a-zA-Z][a-zA-Z0-9:_-]*)\\s*=\\s*\"([^\"]*)\""));

  // 这些属性除了 style 之外还要单独写一份属性：QSvgRenderer 认属性不认 style
  static const char* const kPresAttrs[] = {"fill",
                                           "stroke",
                                           "stroke-width",
                                           "stroke-dasharray",
                                           "stroke-linecap",
                                           "stroke-linejoin",
                                           "fill-rule",
                                           "fill-opacity",
                                           "stroke-opacity",
                                           "opacity",
                                           "font-size",
                                           "font-family",
                                           "font-weight",
                                           "font-style",
                                           "text-anchor",
                                           "dominant-baseline",
                                           "alignment-baseline",
                                           nullptr};

  QString out;
  out.reserve(svg_without_style.size() + 4096);
  QVector<ElementInfo> stack;
  pos = 0;

  it = kTag.globalMatch(svg_without_style);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    out += svg_without_style.mid(pos, match.capturedStart() - pos);
    pos = match.capturedEnd();

    const bool is_close = !match.captured(1).isEmpty();
    const QString tag_name = match.captured(2);
    const QString attrs_str = match.captured(3);
    const bool self_close = !match.captured(4).isEmpty();

    if (is_close) {
      if (!stack.isEmpty()) {
        stack.removeLast();
      }
      out += match.captured(0);
      continue;
    }

    QHash<QString, QString> attrs;
    QStringList attr_order;
    auto attr_it = kAttr.globalMatch(attrs_str);
    while (attr_it.hasNext()) {
      const QRegularExpressionMatch am = attr_it.next();
      const QString name = am.captured(1);
      if (!attrs.contains(name)) {
        attr_order.append(name);
      }
      attrs[name] = am.captured(2);
    }

    ElementInfo current;
    current.tag = tag_name;
    current.id = attrs.value(QStringLiteral("id"));
    current.classes = attrs.value(QStringLiteral("class"))
                          .split(QRegularExpression(QStringLiteral("\\s+")));
    current.classes.removeAll(QString());
    stack.append(current);

    QHash<QString, QString> decls;
    QSet<QString> important;
    ParseDecls(DecodeHtmlEntities(attrs.value(QStringLiteral("style"))), &decls,
               &important);
    // 内联 style 的层叠位次：高于一切"规则 normal"，且内联 !important
    // 高于外部 !important——style 属性的 specificity 是 (1,0,0,0) 级，
    // 在 author 源里就是天花板。这两套键要从初始解析里记下来——后面
    // decls 会被规则逐步覆盖，分不清谁是内联带来的。
    // 不用迭代器构造：QSet(iter, iter) 是 5.15 才有的，本仓支持线是 5.12
    QSet<QString> inline_keys;
    QSet<QString> inline_important;
    for (auto dit = decls.constBegin(); dit != decls.constEnd(); ++dit) {
      inline_keys.insert(dit.key());
      if (important.contains(dit.key())) {
        inline_important.insert(dit.key());
      }
    }

    for (const CssRule& rule : rules) {
      if (!MatchChain(stack, rule)) {
        continue;
      }
      for (auto dit = rule.decls.constBegin(); dit != rule.decls.constEnd();
           ++dit) {
        if (inline_important.contains(dit.key())) {
          continue;
        }
        const bool new_important = rule.important.contains(dit.key());
        const bool old_important = important.contains(dit.key());
        if (old_important && !new_important) {
          continue;
        }
        if (inline_keys.contains(dit.key()) && !new_important) {
          continue;
        }
        decls[dit.key()] = dit.value();
        if (new_important) {
          important.insert(dit.key());
        }
      }
    }

    // 强制字体族：放在所有规则之后，不依赖 Qt 会不会从祖先继承 font-family，
    // 也不受上面这层"文档顺序决胜"的层叠偏差影响
    if (!font_family.isEmpty()) {
      decls[QStringLiteral("font-family")] = font_family;
      important.remove(QStringLiteral("font-family"));
    }

    out += QChar('<') + tag_name;
    QSet<QString> written;
    for (const QString& name : attr_order) {
      if (name == QLatin1String("style")) {
        continue;
      }
      const QString value = decls.contains(name)
                                ? EncodeForXmlAttr(decls.value(name))
                                : attrs.value(name);
      out += QChar(' ') + name + QStringLiteral("=\"") + value + QChar('"');
      written.insert(name);
    }
    for (int i = 0; kPresAttrs[i] != nullptr; ++i) {
      const QString key = QString::fromLatin1(kPresAttrs[i]);
      if (decls.contains(key) && !written.contains(key)) {
        out += QChar(' ') + key + QStringLiteral("=\"") +
               EncodeForXmlAttr(decls.value(key)) + QChar('"');
        written.insert(key);
      }
    }
    const QString merged = BuildStyle(decls, important);
    if (!merged.isEmpty()) {
      out +=
          QStringLiteral(" style=\"") + EncodeForXmlAttr(merged) + QChar('"');
    }
    out += self_close ? QStringLiteral("/>") : QStringLiteral(">");

    if (self_close) {
      stack.removeLast();
    }
  }
  out += svg_without_style.mid(pos);
  return out;
}
