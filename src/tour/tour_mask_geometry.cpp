#include "tour_mask_geometry.h"

#include <QtGlobal>

QRect PlaceMask(const QRect& host, const QSize& minimum,
                const QRect& screen_available) {
  if (host.isNull()) {
    return host;
  }

  // 宿主已经够大：**原样返回**，不重算。别把这条当成优化删掉——下面的"以中心
  // 放大"用的是 QRect::center()（向下取整），尺寸一样时它也会把矩形整体挪 1
  // 像素，而正常窗口（主窗口那条路）的遮罩必须一个像素都不动
  if (host.width() >= minimum.width() && host.height() >= minimum.height()) {
    return screen_available.isNull() ? host
                                     : host.intersected(screen_available);
  }

  const int width = qMax(host.width(), minimum.width());
  const int height = qMax(host.height(), minimum.height());

  // 以**宿主中心**为中心放大：裸并集会把宿主的包围盒顶到左上角，气泡有地方
  // 了画面却是歪的。向下取整那 1 像素的偏差在放大这一支里无所谓（框本来就在
  // 动），但尺寸相同时不行，所以上面先挡住了
  const QPoint center = host.center();
  const QRect mask(center.x() - width / 2, center.y() - height / 2, width,
                   height);

  // 裁到屏幕可用区：不会伸到屏幕外，也顺带处理"宿主已占满屏幕"与小屏
  if (screen_available.isNull()) {
    return mask;
  }
  return mask.intersected(screen_available);
}
