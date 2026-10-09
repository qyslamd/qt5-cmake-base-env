#ifndef FILE_TOUR_COLORS_H_
#define FILE_TOUR_COLORS_H_

#include <QColor>

// 引导自绘层的颜色：遮罩底、气泡底、accent（高亮虚线框与倒计时条）。
//
// 收在这一处是为了让"改哪儿能换皮"有个确定的答案——原先这三个函数挂在
// TourOverlay 上，遮罩与气泡拆成两个类之后，两边都有一半理由带着它们。
// 要做主题适配（跟随应用调色板、日夜两套）改这里即可。
//
// 它们不走 QSS：这几个是 QPainter 自绘的，QSS 管不到（气泡里那些子控件的
// 颜色才走 QSS，见 tour_style.h）
inline QColor TourMaskColor() {
  // 遮罩：半透明黑。alpha 定在这里，深浅要调只改这一个数
  const int alpha = 128;
  return QColor(0, 0, 0, alpha);
}

inline QColor TourBubbleColor() {
  QColor color(0xF0, 0xF0, 0xF0);
  color.setAlpha(245);
  return color;
}

inline QColor TourAccentColor() { return QColor("#007ACC"); }

#endif  // FILE_TOUR_COLORS_H_
