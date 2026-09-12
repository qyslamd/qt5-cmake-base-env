#include "battery_tab_appearance.h"

#include <QLinearGradient>

namespace {

// 电池帽（顶部凸起）：宽度占标签的比例、伸出主体的高度、圆角
constexpr qreal kCapWidthRatio = 0.28;
constexpr qreal kCapHeight = 5.0;
constexpr qreal kCapRadius = 1.5;

// 主体四周内缩，以及圆角
constexpr int kBodyInsetLeft = 3;
constexpr int kBodyInsetRight = 3;
constexpr int kBodyInsetBottom = 3;
constexpr qreal kBodyRadius = 5.0;

// 三态电量：选中满电，悬停半电，普通低电
constexpr qreal kSelectedLevel = 1.0;
constexpr qreal kHoveredLevel = 0.5;
constexpr qreal kNormalLevel = 0.1;

// 色相上限 120° = 绿；0° = 红。饱和度与明度固定，只让色相随电量移动
constexpr int kHueMax = 120;
constexpr int kSaturation = 210;
constexpr int kValue = 220;

// 顶/底渐变明暗倍率，给电池一点圆柱感
constexpr int kTopLighten = 115;
constexpr int kBottomDarken = 115;

}  // namespace

qreal BatteryTabAppearance::levelFor(bool selected, bool hovered) {
  if (selected) {
    return kSelectedLevel;
  }
  if (hovered) {
    return kHoveredLevel;
  }
  return kNormalLevel;
}

QPainterPath BatteryTabAppearance::shape(const QStyleOptionTab* opt,
                                         const TabTheme& theme) const {
  Q_UNUSED(theme);
  const QRectF r = opt->rect;

  // 主体占据底边以上、电池帽以下的部分
  const QRectF body(r.left() + kBodyInsetLeft, r.top() + kCapHeight,
                    r.width() - kBodyInsetLeft - kBodyInsetRight,
                    r.height() - kCapHeight - kBodyInsetBottom);

  // 电池帽居中挂在主体顶边
  const qreal cap_width = r.width() * kCapWidthRatio;
  const QRectF cap(body.center().x() - (cap_width / 2), r.top() + 1, cap_width,
                   kCapHeight + 2);

  QPainterPath body_path;
  body_path.addRoundedRect(body, kBodyRadius, kBodyRadius);

  QPainterPath cap_path;
  cap_path.addRoundedRect(cap, kCapRadius, kCapRadius);

  // 两段并集：帽与主体重叠的部分自然融合，看不出拼接痕迹
  return body_path.united(cap_path);
}

QBrush BatteryTabAppearance::brush(const QStyleOptionTab* opt,
                                   const TabTheme& theme, bool selected,
                                   bool hovered) const {
  Q_UNUSED(theme);
  const QRectF r = opt->rect;

  const qreal level = levelFor(selected, hovered);

  // 色相线性映射：0(红) -> 60(黄) -> 120(绿)
  const int hue = static_cast<int>(level * kHueMax);
  const QColor base = QColor::fromHsv(hue, kSaturation, kValue);

  // 顶亮底暗，让电池有圆柱感
  QLinearGradient g(r.topLeft(), r.bottomLeft());
  g.setColorAt(0.0, base.lighter(kTopLighten));
  g.setColorAt(1.0, base.darker(kBottomDarken));
  return QBrush(g);
}