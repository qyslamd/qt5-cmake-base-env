#ifndef FILE_EXAMPLES_TOUR_DEMO_DEMO_FLOWS_H_
#define FILE_EXAMPLES_TOUR_DEMO_DEMO_FLOWS_H_

class QWidget;

class TourController;

// 给演示窗口注册三条主题，分别覆盖引导引擎的一类能力：
//
//   基本控件导览  讲解式：逐个控件高亮 + 自动倒计时 + 空目标收尾步
//   动手试试      实操式（interactive）：遮罩对鼠标穿透，等用户自己动手，
//                 不自动翻页
//   区域和插图    另外两种目标类型：QRect（全局坐标的一块区域）与
//                 QPixmap（居中显示一张图）
//
// 顺序有讲究：实操式那条夹在中间，配合主页的「全部演示」正好验证
// "走过实操主题之后，后续讲解主题能不能回到自动"
void AddDemoTopics(TourController* controller, QWidget* root);

#endif  // FILE_EXAMPLES_TOUR_DEMO_DEMO_FLOWS_H_
