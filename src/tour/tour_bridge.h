#ifndef FILE_TOUR_BRIDGE_H_
#define FILE_TOUR_BRIDGE_H_

#include <QPointer>
#include <QVariant>
#include <QWidget>

class TourOverlay;

// 外部驱动的桥接层：让测试工具从**进程外**高亮界面上的控件——脚本走到哪一步，
// 屏幕上就指到哪一步。
//
// 这是**可选的适配层**：核心引擎（Controller / Presentation）不依赖它，宿主也
// 可以不用。这里的实现以 funq 为例，三条形状上的要求都来自它；换用别的驱动工具
// 时照这个类的样子重写一份即可：
//
//   为什么是**可见的 QWidget** 而不是普通 QObject
//     funq 在服务端对目标做 qobject_cast<QWidget*>，纯 QObject 会被判成
//     NotAWidget；客户端还会等控件 visible。本类没有任何界面，是 0 尺寸、
//     不挡鼠标的一块占位控件
//
//   为什么是**独立顶层窗口**
//     脚本的第一步常常跑在宿主主窗口还没创建的时候（比如登录对话框上），
//     挂在主窗口下的话那几步没法高亮
//
//   为什么槽签名是"单个 QVariant 进、QVariant 出"
//     funq 固定按 QMetaObject::invokeMethod(obj, name, DirectConnection,
//     Q_RETURN_ARG(QVariant), Q_ARG(QVariant)) 调；拆成多个形参匹配不上，
//     直接返回 NoMethodInvoked
//
// 参数用 map：{"path":…, "title":…, "text":…}；
// 返回 {"ok": bool, "message": str}，调用方可据此判断，不会静默。
class TourBridge : public QWidget {
  Q_OBJECT

 public:
  // overlay 由控制器持有，本类只引用它：桥接要做的只是"套被动模式 +
  // 指一个控件"，不碰主题/步骤那套推进逻辑
  explicit TourBridge(TourOverlay* overlay,
                          QWidget* parent = nullptr);

  Q_INVOKABLE QVariant showHighlight(const QVariant& params);
  Q_INVOKABLE QVariant hideHighlight(const QVariant& params);

 private:
  QPointer<TourOverlay> overlay_;
};

#endif  // FILE_TOUR_BRIDGE_H_
