#ifndef FILE_LIBUI_TAB_APPEARANCE_TAB_APPEARANCE_H_
#define FILE_LIBUI_TAB_APPEARANCE_TAB_APPEARANCE_H_

#include <QBrush>
#include <QColor>
#include <QLineF>
#include <QPainterPath>
#include <QSize>
#include <QStyleOptionTab>

// 独立于外观的配色/尺寸主题。文字色由 TabBarStyle 绘制标签文字时使用
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
                           const QSize& base_size) const {
    Q_UNUSED(opt);
    return base_size;
  }

  // 生成 tab 形状。opt->rect 是 QTabBar 分配给 tab 的完整矩形
  virtual QPainterPath shape(const QStyleOptionTab* opt,
                             const TabTheme& theme) const = 0;

  // 填充画刷。默认取主题的选中/悬停纯色；子类可覆盖成渐变等自定义画刷。
  // 返回 NoBrush 表示该状态不填充
  virtual QBrush brush(const QStyleOptionTab* opt, const TabTheme& theme,
                       bool selected, bool hovered) const {
    Q_UNUSED(opt);
    if (selected) {
      return QBrush(theme.selectedBackground);
    }
    if (hovered) {
      return QBrush(theme.hoverBackground);
    }
    return QBrush(Qt::NoBrush);
  }

  // 未选中且未悬停的 tab 是否要画分隔线
  virtual bool wantsSeparator(const QStyleOptionTab* opt) const {
    Q_UNUSED(opt);
    return false;
  }

  virtual QLineF separatorLine(const QStyleOptionTab* opt) const {
    Q_UNUSED(opt);
    return QLineF();
  }

  // ---- 动画钩子 ----

  // 返回 true 时 TabBarStyle 启动逐帧定时器驱动本外观
  virtual bool isAnimated() const { return false; }

  // 每帧调用一次，推进外观自身的动画相位
  virtual void tick() {}
};

#endif  // FILE_LIBUI_TAB_APPEARANCE_TAB_APPEARANCE_H_
