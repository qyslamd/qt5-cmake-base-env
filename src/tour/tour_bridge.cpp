#include "tour_bridge.h"

#include <tour_overlay.h>

#include <QApplication>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTabBar>
#include <QWidget>
#include <QtDebug>

namespace {

// 路径解析。规则**照 funq 的 ObjectPath**（libFunq/objectpath.cpp）重写：
//
//   名字   = objectName；为空时退到类名（所以路径里会有 "QWidget" 这种段）
//   唯一名 = 同名兄弟里非首个的带 "-<序号>" 后缀（settingsRow-1 就是这么来的）
//   路径   = 各段用 "::" 拼，解析时逐段下钻
//
// 自己写而不是把 funq 的源码拷进来：funq 是 CeCILL v2.1（copyleft），拷进
// src/tour 会把我们自己的库一起卷进那个许可证。算法本身不受版权保护，
// 照着重写既干净又短，而且仍然**零 funq 依赖**
QString RawObjectName(QObject* object) {
  QString name = object->objectName();
  if (name.isEmpty() && object->metaObject() != nullptr) {
    name = QString::fromLatin1(object->metaObject()->className());
  }
  return name;
}

// 单个对象在路径里的名字，含同名兄弟的序号后缀
QString NameInPath(QObject* object) {
  QString name = RawObjectName(object);

  const QObject* parent = object->parent();
  if (parent != nullptr) {
    int index = 0;
    const QList<QObject*> siblings = parent->children();
    for (QObject* sibling : siblings) {
      if (sibling == object) {
        break;
      }
      if (RawObjectName(sibling) == name) {
        ++index;
      }
    }
    if (index > 0) {
      name = QStringLiteral("%1-%2").arg(name).arg(index);
    }
  }

  // "::" 是路径分隔符，名字里若含它就转义掉（与 funq 一致）
  name.replace(QStringLiteral("::"), QStringLiteral(":_:"));
  return name;
}

QObject* FindObjectByPath(const QString& path) {
  QStringList parts = path.split(QStringLiteral("::"));
  if (parts.isEmpty()) {
    return nullptr;
  }

  const QString name = parts.takeLast();

  if (parts.isEmpty()) {
    // 顶层：从应用的所有顶层窗口里找
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (QWidget* widget : tops) {
      if (widget != nullptr && NameInPath(widget) == name) {
        return widget;
      }
    }
    return nullptr;
  }

  QObject* parent = FindObjectByPath(parts.join(QStringLiteral("::")));
  if (parent == nullptr) {
    return nullptr;
  }
  const QList<QObject*> children = parent->children();
  for (QObject* child : children) {
    if (child != nullptr && NameInPath(child) == name) {
      return child;
    }
  }
  return nullptr;
}

// 引导只能高亮 QWidget（引擎的目标类型只有 控件 / 矩形 / 图），
// 所以菜单项那种 QAction 到这里会被挡掉
QWidget* ResolveWidget(const QString& path) {
  return qobject_cast<QWidget*>(FindObjectByPath(path));
}

}  // namespace

TourBridge::TourBridge(TourOverlay* overlay, QWidget* parent)
    : QWidget(parent), overlay_(overlay) {
  // funq 靠 objectName 定位到本对象再 call_slot。Tool 标志让它不进任务栏，
  // 0 尺寸 + 不挡鼠标，用户看不见它——它只是 funq 的落脚点。
  // show() 不能省：客户端默认等控件 visible 才返回，不 show 会一直超时
  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
  setObjectName(QStringLiteral("tourBridge"));
  resize(0, 0);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  show();
}

QVariant TourBridge::showHighlight(const QVariant& params) {
  QVariantMap result;
  if (overlay_ == nullptr) {
    result["ok"] = false;
    result["message"] = QStringLiteral("演示层尚未就绪");
    return result;
  }

  const QVariantMap args = params.toMap();
  // 多靶（《一步多靶设计》§3.3）：`paths` 是数组，[0] 是锚点、其余是额外靶。
  // 与单数 `path` 同时给时**按 paths 处理并记一条警告**——两个键混着写多半
  // 是笔误，静默挑一个会让人查半天
  QStringList path_list;
  const QVariant paths_value = args.value(QStringLiteral("paths"));
  if (!paths_value.toStringList().isEmpty()) {
    path_list = paths_value.toStringList();
    if (!args.value(QStringLiteral("path")).toString().isEmpty()) {
      qWarning() << "引导桥接：path 与 paths 同时给了，按 paths 处理";
    }
  } else {
    path_list << args.value(QStringLiteral("path")).toString();
  }

  const QString path = path_list.value(0);
  QWidget* target = ResolveWidget(path);
  if (target == nullptr) {
    // 解析失败要说出来：脚本里写错一个 objectName 是常事，静默没有任何反馈
    // 会让人以为"桥接根本没通"
    result["ok"] = false;
    result["message"] =
        QStringLiteral("按路径找不到控件（路径要写成 funq 那种完整路径）: %1")
            .arg(path);
    qWarning() << "引导桥接：按路径找不到控件" << path;
    return result;
  }

  // 不可见的控件不给高亮：它还没被布局过，几何是未初始化的，画出来的框会跑到
  // 屏幕角落去（菜单栏里的 QMenu 就是典型——从没显示过，报出来的位置是 0,0）。
  // 宁可报失败，也不要画一个假的框
  if (!target->isVisible() || target->size().isEmpty()) {
    result["ok"] = false;
    result["message"] =
        QStringLiteral("目标不可见或尺寸为空，不给高亮: %1").arg(path);
    return result;
  }

  // 额外靶：与锚点同一套校验（找得到、可见、尺寸非空），外加**必须与锚点同
  // 窗口**——不同窗口的控件映到遮罩坐标系之后落在遮罩之外，屏幕上看不见框而
  // 这里照样回报成功，与"不静默"的口径相反
  QList<QWidget*> extras;
  const QWidget* anchor_window = target->window();
  for (int i = 1; i < path_list.size(); ++i) {
    const QString extra_path = path_list.at(i);
    QWidget* extra = ResolveWidget(extra_path);
    if (extra == nullptr) {
      result["ok"] = false;
      result["message"] = QStringLiteral("第 %1 个额外靶按路径找不到控件: %2")
                              .arg(i)
                              .arg(extra_path);
      qWarning() << "引导桥接：额外靶找不到控件" << extra_path;
      return result;
    }
    if (!extra->isVisible() || extra->size().isEmpty()) {
      result["ok"] = false;
      result["message"] = QStringLiteral("第 %1 个额外靶不可见或尺寸为空: %2")
                              .arg(i)
                              .arg(extra_path);
      return result;
    }
    if (extra->window() != anchor_window) {
      result["ok"] = false;
      result["message"] =
          QStringLiteral("第 %1 个额外靶与锚点不在同一个窗口里: %2")
              .arg(i)
              .arg(extra_path);
      return result;
    }
    extras.append(extra);
  }

  // 被动模式：只显示、不交互，推进全由外部脚本给
  overlay_->setPassiveMode(true);

  const QString title = args.value(QStringLiteral("title")).toString();
  const QString text = args.value(QStringLiteral("text")).toString();
  // 可选：指页签条上的第 N 个页签。**页签不是 QWidget**（QTabBar 的一个 tab
  // 只是画出来的），按 path 找不到它——用 tabRect() 算出矩形，走按矩形高亮
  // 那条路。不传这个键就是老行为：指整个控件
  const int tab_index = args.value(QStringLiteral("tab"), -1).toInt();
  if (tab_index >= 0) {
    QTabBar* bar = qobject_cast<QTabBar*>(target);
    if (bar == nullptr) {
      result["ok"] = false;
      result["message"] =
          QStringLiteral("给了 tab，但按路径找到的不是页签条: %1").arg(path);
      return result;
    }
    if (tab_index >= bar->count()) {
      result["ok"] = false;
      result["message"] = QStringLiteral("页签下标越界：%1 只有 %2 个页签")
                              .arg(path)
                              .arg(bar->count());
      return result;
    }
    const QRect tab_rect = bar->tabRect(tab_index);
    if (tab_rect.isEmpty()) {
      result["ok"] = false;
      result["message"] =
          QStringLiteral("第 %1 个页签的矩形是空的（它被藏起来了？）: %2")
              .arg(tab_index)
              .arg(path);
      return result;
    }
    // tab 只作用于**锚点**（页签不是控件，当不成额外靶）；给 tab 时锚点走矩形
    // 那条路，额外靶仍是控件、由 showHighlightRect 的 extras 收
    QList<QRect> extra_rects;
    for (QWidget* extra : extras) {
      extra_rects.append(
          QRect(extra->mapToGlobal(QPoint(0, 0)), extra->size()));
    }
    overlay_->showHighlightRect(
        bar->window(),
        QRect(bar->mapToGlobal(tab_rect.topLeft()), tab_rect.size()), title,
        text, extra_rects);
  } else {
    // 额外靶走**控件**这条路，不预先转成矩形：showHighlight 里有
    // setHost(..., inflate = target == target_window)，整窗高亮靠它才不至于
    // "整块遮罩被掏空、只剩气泡"（先转矩形会把这一条绕过去）
    overlay_->showHighlight(target, title, text, extras);
  }

  // 可选：视觉倒计时（秒）。只走显示、**不推进任何东西**——下一步做什么由
  // 脚本决定，这里只让"马上要切了"看得见
  const int countdown = args.value(QStringLiteral("countdown"), 0).toInt();
  if (countdown > 0) {
    overlay_->showCountdown(countdown);
  }

  result["ok"] = true;
  result["message"] = QStringLiteral("高亮 %1").arg(path);
  // 把算出来的数字一并回报：高亮/遮罩算在哪儿，出问题时只能靠这个定位。
  // `rect` 只报**锚点**——框可能有多个，只看这一个分不出"少框了一个"，
  // 所以另给一个 `count`
  const QRect rect = overlay_->highlightRect();
  const QRect overlay = overlay_->geometry();
  result["count"] = overlay_->highlightCount();
  result["rect"] = QStringLiteral("%1,%2 %3x%4")
                       .arg(rect.x())
                       .arg(rect.y())
                       .arg(rect.width())
                       .arg(rect.height());
  result["overlay"] = QStringLiteral("%1,%2 %3x%4")
                          .arg(overlay.x())
                          .arg(overlay.y())
                          .arg(overlay.width())
                          .arg(overlay.height());
  result["host"] =
      target->window() == nullptr
          ? QStringLiteral("(无)")
          : QStringLiteral("%1 %2,%3 %4x%5")
                .arg(target->window()->objectName().isEmpty()
                         ? target->window()->metaObject()->className()
                         : target->window()->objectName())
                .arg(target->window()->mapToGlobal(QPoint(0, 0)).x())
                .arg(target->window()->mapToGlobal(QPoint(0, 0)).y())
                .arg(target->window()->width())
                .arg(target->window()->height());
  return result;
}

QVariant TourBridge::hideHighlight(const QVariant& params) {
  Q_UNUSED(params);

  QVariantMap result;
  if (overlay_ == nullptr) {
    result["ok"] = false;
    result["message"] = QStringLiteral("演示层尚未就绪");
    return result;
  }
  overlay_->clearHighlight();
  result["ok"] = true;
  result["message"] = QStringLiteral("已清除高亮");
  return result;
}
