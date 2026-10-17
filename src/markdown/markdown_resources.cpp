#include "markdown_resources.h"

#include <QtCore/qglobal.h>  // Q_INIT_RESOURCE

// Q_INIT_RESOURCE 展开后是一个 extern 声明加一次调用，**必须留在文件作用域、
// 且不能放进匿名命名空间**——那样声明会绑到匿名空间上，链接期找不到 rcc
// 生成的全局函数（src/style、src/tour 都踩过同一条）。qrc 名只在这里写一次。
// 各渲染器在自己的 EnsureLoaded 里调，不依赖"先有人建过 MarkdownView"这个
// 前提——单测直接调渲染函数时就没有视图
void EnsureMarkdownResources() { Q_INIT_RESOURCE(markdown_res); }
