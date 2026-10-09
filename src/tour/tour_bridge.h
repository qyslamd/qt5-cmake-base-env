#ifndef FILE_TOUR_BRIDGE_H_
#define FILE_TOUR_BRIDGE_H_

#include <QPointer>
#include <QVariant>
#include <QWidget>

class TourOverlay;

// 外部驱动的桥接层（以 funq 为例）：脚本从进程外高亮界面上的控件。可选的
// 适配层，核心引擎（Controller / Overlay）不依赖它。
// 三条形状是 funq 的硬要求，改了这个类就调不动了：**可见的 QWidget**（它
// qobject_cast 目标并等 visible）、**独立顶层窗口**（脚本常在主窗口还没建起来
// 时就要高亮）、**槽一个 QVariant 进一个出**（它按这个签名 invokeMethod）
class TourBridge : public QWidget {
  Q_OBJECT

 public:
  // overlay 由控制器持有，本类只引用它：桥接只做"套被动模式 + 指一个控件"
  explicit TourBridge(TourOverlay* overlay, QWidget* parent = nullptr);

  // 槽名、入参键名、返回值的键名都是脚本按名字读的对外契约，改名即失效。
  // 入参 {"path", "title", "text", ["paths"], ["tab"], ["countdown"]}：
  // `paths` 是一步多靶（[0] 锚点，其余一起框住），与 `path` 同给时按 paths
  // 处理；`tab` 只作用于锚点（页签不是 QWidget）。返回 {"ok", "message",
  // "rect", "count", "overlay", "host"}，`rect` 只报锚点，框数看 `count`
  Q_INVOKABLE QVariant showHighlight(const QVariant& params);
  Q_INVOKABLE QVariant hideHighlight(const QVariant& params);

 private:
  QPointer<TourOverlay> overlay_;
};

#endif  // FILE_TOUR_BRIDGE_H_
