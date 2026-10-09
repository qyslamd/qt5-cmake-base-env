#ifndef FILE_TOUR_COLORS_H_
#define FILE_TOUR_COLORS_H_

#include <QColor>

// 引导自绘层的颜色：遮罩底、气泡底、accent（高亮虚线框与倒计时条）。
// 收在一处，换皮（跟随调色板、日夜两套）只改这里。
//
// 它们不走 QSS——这几个是 QPainter 自绘的，QSS 管不到（气泡里子控件的
// 颜色才走 QSS，见 tour_style.h）
inline QColor TourMaskColor() {
  // 遮罩：半透明黑，深浅只改这一个数
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
