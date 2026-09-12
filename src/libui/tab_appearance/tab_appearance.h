#ifndef FILE_LIBUI_TAB_APPEARANCE_TAB_APPEARANCE_H_
#define FILE_LIBUI_TAB_APPEARANCE_TAB_APPEARANCE_H_

#include <QColor>
#include <QLineF>
#include <QPainterPath>
#include <QSize>
#include <QStyleOptionTab>

// 独立于外观的配色/尺寸主题
struct TabTheme {
  QColor selectedBackground = QColor(123, 104, 238);
  QColor hoverBackground = QColor(180, 180, 180, 120);
  QColor separatorColor = QColor(140, 140, 140);
  QColor selectedTextColor = Qt::white;
  QColor normalTextColor = Qt::black;
  int tabHeight = 36;
};

// 抽象外观：只回答"长什么样"
class TabAppearance {
 public:
  virtual ~TabAppearance() = default;

  // 在 QStyle 默认 sizeFromContents 基础上调整
  virtual QSize adjustSize(const QStyleOptionTab* opt,
                           const QSize& baseSize) const {
    Q_UNUSED(opt);
    return baseSize;
  }

  // 生成 tab 形状。opt->rect 是 QTabBar 分配给 tab 的完整矩形。
  virtual QPainterPath shape(const QStyleOptionTab* opt,
                             const TabTheme& theme) const = 0;

  // 未选中且未 hover 的 tab 是否要画分隔线
  virtual bool wantsSeparator(const QStyleOptionTab* opt) const {
    Q_UNUSED(opt);
    return false;
  }

  virtual QLineF separatorLine(const QStyleOptionTab* opt) const {
    Q_UNUSED(opt);
    return QLineF();
  }
};

#endif  // FILE_LIBUI_TAB_APPEARANCE_TAB_APPEARANCE_H_