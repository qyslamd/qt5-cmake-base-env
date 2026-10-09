#ifndef FILE_TOUR_MASK_GEOMETRY_H_
#define FILE_TOUR_MASK_GEOMETRY_H_

#include <QRect>
#include <QSize>

// 遮罩铺到哪儿：**宿主矩形与"最小视口"取大，再裁到屏幕可用区**。纯几何，无控件。
//
// 两个需求在打架：语义上遮罩要贴着宿主（挖洞、虚线框、点空白退出都在宿主那套
// 坐标里），而它同时又是气泡的坐标系原点——气泡的"视口"就是遮罩的尺寸。宿主
// 比气泡还小时（登录框 460×370，气泡最小 300×200），四个方向都放不下，气泡
// 只能压着目标、或者挤在角落。
//
// 宿主本来就比 minimum 大时，结果就是宿主本身——普通窗口（主窗口 1024×900）
// 的遮罩一点没变。放大的那部分会盖住宿主周围一圈桌面，"点空白 = 退出"的空间
// 含义随之变大：那正是"点引导区之外就是我不看了"
QRect PlaceMask(const QRect& host, const QSize& minimum,
                const QRect& screen_available);

#endif  // FILE_TOUR_MASK_GEOMETRY_H_
