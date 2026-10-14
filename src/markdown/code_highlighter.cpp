#include "code_highlighter.h"

#include <QHash>
#include <QSet>
#include <QStringList>

namespace {

// 一种语言的词法要素。关键字写成空格分隔的一串，省得摆一堆 QStringLiteral
struct HighlightRules {
  QString keywords;
  QString line_comment;  // 空 = 这种语言没有行注释
  bool block_comment = false;
};

QHash<QString, HighlightRules> BuildRules() {
  const HighlightRules cpp{
      QStringLiteral(
          "if else for while do switch case default break continue return goto "
          "class struct union enum namespace using template typename public "
          "private protected virtual override final static const constexpr "
          "mutable volatile inline explicit friend operator new delete this "
          "try catch throw noexcept sizeof alignof decltype auto void bool "
          "char short int long float double signed unsigned wchar_t nullptr "
          "true false static_cast dynamic_cast const_cast reinterpret_cast "
          "include define ifndef ifdef endif pragma"),
      QStringLiteral("//"), true};
  const HighlightRules cmake{
      QStringLiteral(
          "if elseif else endif foreach endforeach function endfunction macro "
          "endmacro while endwhile break continue return set unset list string "
          "file math option message include find_package project add_library "
          "add_executable add_subdirectory add_custom_command "
          "add_custom_target "
          "target_link_libraries target_include_directories "
          "target_compile_definitions target_compile_options "
          "target_sources target_compile_features install "
          "set_target_properties "
          "cmake_minimum_required cmake_parse_arguments and or not in"),
      QStringLiteral("#"), false};
  const HighlightRules shell{
      QStringLiteral(
          "if then else elif fi for while until do done case esac function "
          "return break continue local export readonly declare unset source "
          "echo printf cd exit set test in"),
      QStringLiteral("#"), false};
  const HighlightRules python{
      QStringLiteral(
          "def class if elif else for while return import from as try except "
          "finally with lambda yield global nonlocal pass break continue and "
          "or "
          "not in is None True False self async await assert del raise"),
      QStringLiteral("#"), false};
  const HighlightRules json{QStringLiteral("true false null"), QString(),
                            false};
  // CSS/QSS 只认块注释：属性名不当关键字，省得一屏全是高亮
  const HighlightRules css{QString(), QString(), true};

  QHash<QString, HighlightRules> rules;
  for (const QString& name :
       {QStringLiteral("cpp"), QStringLiteral("c"), QStringLiteral("cc"),
        QStringLiteral("cxx"), QStringLiteral("h"), QStringLiteral("hpp"),
        QStringLiteral("hxx")}) {
    rules.insert(name, cpp);
  }
  rules.insert(QStringLiteral("cmake"), cmake);
  for (const QString& name :
       {QStringLiteral("bash"), QStringLiteral("sh"), QStringLiteral("bat"),
        QStringLiteral("cmd"), QStringLiteral("powershell"),
        QStringLiteral("ps1")}) {
    rules.insert(name, shell);
  }
  for (const QString& name : {QStringLiteral("python"), QStringLiteral("py")}) {
    rules.insert(name, python);
  }
  rules.insert(QStringLiteral("json"), json);
  for (const QString& name : {QStringLiteral("qss"), QStringLiteral("css")}) {
    rules.insert(name, css);
  }
  return rules;
}

const HighlightRules& RulesFor(const QString& language) {
  static const QHash<QString, HighlightRules> kRules = BuildRules();
  const auto found = kRules.constFind(language.toLower());
  if (found != kRules.cend()) {
    return found.value();
  }
  return kRules.constFind(QStringLiteral("cpp")).value();
}

QSet<QString> KeywordSet(const QString& keywords) {
  QSet<QString> set;
  const QStringList words =
      keywords.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (const QString& word : words) {
    set.insert(word);
  }
  return set;
}

QString EscapeHtml(const QString& text) {
  QString out = text;
  out.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
  out.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
  out.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
  return out;
}

QString Span(const QString& css_class, const QString& text) {
  return QStringLiteral("<span class=\"%1\">%2</span>")
      .arg(css_class, EscapeHtml(text));
}

}  // namespace

QString HighlightCode(const QString& code, const QString& language) {
  if (code.isEmpty()) {
    return {};
  }

  const HighlightRules& rules = RulesFor(language);
  const QSet<QString> keywords = KeywordSet(rules.keywords);
  // C 系的多行注释要跨行保持状态，其余语言用不上
  bool in_block_comment = false;

  QString out;
  const QStringList lines = code.split(QLatin1Char('\n'));
  for (int line_index = 0; line_index < lines.size(); ++line_index) {
    const QString& line = lines[line_index];
    int i = 0;
    while (i < line.size()) {
      if (in_block_comment) {
        const int end = line.indexOf(QStringLiteral("*/"), i);
        if (end < 0) {
          out += Span(QStringLiteral("cmt"), line.mid(i));
          i = line.size();
        } else {
          out += Span(QStringLiteral("cmt"), line.mid(i, end - i + 2));
          i = end + 2;
          in_block_comment = false;
        }
        continue;
      }

      // 行注释：空串会匹配一切，所以这条判断要先看规则里有没有注释符
      if (!rules.line_comment.isEmpty() &&
          line.mid(i, rules.line_comment.size()) == rules.line_comment) {
        out += Span(QStringLiteral("cmt"), line.mid(i));
        i = line.size();
        continue;
      }

      if (rules.block_comment && line.mid(i, 2) == QStringLiteral("/*")) {
        out += Span(QStringLiteral("cmt"), QStringLiteral("/*"));
        i += 2;
        in_block_comment = true;
        continue;
      }

      const QChar current = line[i];

      if (current == QLatin1Char('"') || current == QLatin1Char('\'')) {
        const QChar quote = current;
        int j = i + 1;
        while (j < line.size() && line[j] != quote) {
          if (line[j] == QLatin1Char('\\')) {
            ++j;
          }
          ++j;
        }
        if (j < line.size()) {
          ++j;  // 收尾引号
        }
        out += Span(QStringLiteral("str"), line.mid(i, j - i));
        i = j;
        continue;
      }

      if (current.isLetter() || current == QLatin1Char('_')) {
        int j = i;
        while (j < line.size() &&
               (line[j].isLetterOrNumber() || line[j] == QLatin1Char('_'))) {
          ++j;
        }
        const QString word = line.mid(i, j - i);
        out += keywords.contains(word) ? Span(QStringLiteral("kw"), word)
                                       : EscapeHtml(word);
        i = j;
        continue;
      }

      if (current.isDigit()) {
        int j = i;
        while (j < line.size() &&
               (line[j].isDigit() || line[j] == QLatin1Char('.'))) {
          ++j;
        }
        out += Span(QStringLiteral("num"), line.mid(i, j - i));
        i = j;
        continue;
      }

      out += EscapeHtml(QString(current));
      ++i;
    }

    if (line_index < lines.size() - 1) {
      out += QLatin1Char('\n');
    }
  }
  return out;
}
