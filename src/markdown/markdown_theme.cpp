#include "markdown_theme.h"

namespace {

const char* const kThemeCss[kMarkdownThemeCount] = {
    ":/markdown/themes/light.css",
    ":/markdown/themes/dark.css",
};

}  // namespace

// 加主题时这里少了 case 会静默落到 Light；枚举规模与 kMarkdownThemeCount
// 对不上则编译失败，逼着同步整张表
static_assert(static_cast<int>(MarkdownTheme::Dark) + 1 == kMarkdownThemeCount,
              "新增主题后同步 kMarkdownThemeCount / ThemeIndex / 主题表");

int ThemeIndex(MarkdownTheme theme) {
  switch (theme) {
    case MarkdownTheme::Light:
      return 0;
    case MarkdownTheme::Dark:
      return 1;
  }
  return 0;
}

QString ThemeCssResource(MarkdownTheme theme) {
  return QString::fromLatin1(kThemeCss[ThemeIndex(theme)]);
}

QColor ThemeFormulaColor(MarkdownTheme theme) {
  switch (theme) {
    case MarkdownTheme::Light:
      return QColor(0x24, 0x29, 0x2e);
    case MarkdownTheme::Dark:
      return QColor(0xe6, 0xed, 0xf3);
  }
  return QColor(0x24, 0x29, 0x2e);
}
