#ifndef FILE_TOUR_STYLE_H_
#define FILE_TOUR_STYLE_H_

// 引导自带的样式表。
//
// 样式要作用在**应用级**而不是逐个控件上：气泡在实操模式下会被 reparent 到
// viewport，离开首页的子树，挂在首页上的样式就盖不到它了。所以这里统一并入
// 应用样式表，一次覆盖全部引导界面。
//
// 同时它也是库资源的初始化点——静态库里的 qrc 生成的符号不会被自动链接进来，
// 得有代码显式引用一次。放在库内部，宿主就不必知道资源叫什么（漏写的代价是
// 静默丢样式：QFile 打不开、样式为空、不报错）。
class TourStyle {
 public:
  // 首次调用时把引导样式追加进应用样式表，可重复调用
  static void EnsureApplied();
};

#endif  // FILE_TOUR_STYLE_H_
