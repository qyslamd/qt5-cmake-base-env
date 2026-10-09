#ifndef FILE_TOUR_STYLE_H_
#define FILE_TOUR_STYLE_H_

// 引导样式表的入口，也是库资源的初始化点。
// 样式设在**应用级**：实操模式下气泡会被 reparent 到 viewport，离开首页的
// 子树，挂在首页上的样式就盖不到它了。静态库里的 qrc 符号不会被自动链接，
// 需要一次显式引用——两件事都由 ensureApplied() 收口
class TourStyle {
 public:
  // 首次调用时把引导样式追加进应用样式表（可重复调用）
  static void ensureApplied();
};

#endif  // FILE_TOUR_STYLE_H_
