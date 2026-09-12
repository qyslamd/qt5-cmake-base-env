#include "neon_tab_appearance.h"

#include <QPainter>
#include <QtMath>

namespace {

// 呼吸相位步长
constexpr qreal kPhaseStep = 0.08;

// 辉光的每一层：线宽与不透明度由宽到窄、由淡到亮地叠出扩散感。
// 这不是真模糊，但省掉了 QGraphicsBlurEffect 的渲染成本
struct GlowLayer {
  qreal width;
  qreal alpha;
};

constexpr GlowLayer kGlowLayers[] = {
    {14.0, 0.05}, {10.0, 0.09}, {7.0, 0.15}, {5.0, 0.24}, {3.0, 0.42},
};

// 灯芯线宽，以及叠在它上面的白芯线宽与不透明度
constexpr qreal kCoreWidth = 1.8;
constexpr qreal kWhiteCoreWidth = 0.8;
constexpr qreal kWhiteCoreAlpha = 0.85;

// 选中态的呼吸深度：亮度在 kBreathFloor 与 kBreathFloor + kBreathRange 之间往返
constexpr qreal kBreathFloor = 0.75;
constexpr qreal kBreathRange = 0.25;

// 圆角半径，以及形状四周留出、给辉光散开的空隙（左右上底，单位像素）
constexpr qreal kCornerRadius = 6.0;
constexpr int kInsetLeft = 3;
constexpr int kInsetTop = 4;
constexpr int kInsetRight = 3;
constexpr int kInsetBottom = 1;

// 三态配色。辉光必须有暗底才亮得起来，浅色标签条上只会发灰
const QColor kBackdrop(0x06, 0x06, 0x0C);
const QColor kSelectedNeon(0x00, 0xFF, 0xFF);
const QColor kHoveredNeon(0xFF, 0x33, 0xCC);
const QColor kNormalNeon(0x88, 0xAA, 0xFF);
const QColor kSelectedInner(0x08, 0x12, 0x1E);
const QColor kHoveredInner(0x14, 0x0A, 0x1E);
const QColor kNormalInner(0x0C, 0x0C, 0x14);

}  // namespace

void NeonTabAppearance::tick() {
  phase_ += kPhaseStep;
  if (phase_ > 2.0 * M_PI) {
    phase_ -= 2.0 * M_PI;
  }
}

QPainterPath NeonTabAppearance::shape(const QStyleOptionTab* opt,
                                      const TabTheme& theme) const {
  Q_UNUSED(theme);
  QRectF r = opt->rect;
  r.adjust(kInsetLeft, kInsetTop, -kInsetRight, -kInsetBottom);

  QPainterPath path;
  path.addRoundedRect(r, kCornerRadius, kCornerRadius);
  return path;
}

void NeonTabAppearance::paint(QPainter* p, const QStyleOptionTab* opt,
                              const TabTheme& theme, bool selected,
                              bool hovered) const {
  const QPainterPath path = shape(opt, theme);

  QColor neon = kNormalNeon;
  QColor inner = kNormalInner;
  if (selected) {
    neon = kSelectedNeon;
    inner = kSelectedInner;
  } else if (hovered) {
    neon = kHoveredNeon;
    inner = kHoveredInner;
  }

  // 只有选中态呼吸，其余状态恒定，免得整条标签栏一直闪
  qreal breath = 1.0;
  if (selected) {
    breath = kBreathFloor + (kBreathRange * (0.5 + (0.5 * qSin(phase_))));
  }

  p->save();
  p->setRenderHint(QPainter::Antialiasing);
  p->setPen(Qt::NoPen);

  // 整条标签先铺深色底。辉光会溢出形状边界，不铺底的话这些溢出的部分
  // 落在浅色标签栏上会发灰，看不出霓虹
  p->setBrush(kBackdrop);
  p->drawRect(opt->rect);

  // 灯管内部比底色再深一点，形成管壁的层次
  p->setBrush(inner);
  p->drawPath(path);

  // 辉光：多层描边由宽到窄叠加
  for (const GlowLayer& layer : kGlowLayers) {
    QColor glow = neon;
    glow.setAlphaF(layer.alpha * breath);
    p->setPen(
        QPen(glow, layer.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);
  }

  // 灯芯：一圈本色，再叠一圈白芯，模拟灯丝
  QColor core = neon;
  core.setAlphaF(breath);
  p->setPen(QPen(core, kCoreWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p->drawPath(path);

  QColor white_core(Qt::white);
  white_core.setAlphaF(kWhiteCoreAlpha * breath);
  p->setPen(QPen(white_core, kWhiteCoreWidth, Qt::SolidLine, Qt::RoundCap,
                 Qt::RoundJoin));
  p->drawPath(path);

  p->restore();
}
