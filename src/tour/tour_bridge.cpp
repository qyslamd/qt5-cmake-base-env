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

// 路径解析，规则照 funq 的 ObjectPath 重写：名字取 objectName（为空退到类名，
// 所以路径里会有 "QWidget" 这种段），同名兄弟里非首个带 "-<序号>" 后缀，各段用
// "::" 拼、逐段下钻。**不拷 funq 的源码**：它是 CeCILL v2.1（copyleft），拷进来
// 会把这边的库一起卷进那个许可证
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
  const QStringList parts = path.split(QStringLiteral("::"));
  if (parts.isEmpty()) {
    return nullptr;
  }

  // 第一段从应用的所有顶层窗口里找——顶层没有父，只能单独扫一遍
  QObject* found = nullptr;
  const QWidgetList tops = QApplication::topLevelWidgets();
  for (QWidget* widget : tops) {
    if (widget != nullptr && NameInPath(widget) == parts.at(0)) {
      found = widget;
      break;
    }
  }

  // 其余各段逐级往下（循环即可，不必每层把剩下的段重新拼回字符串）
  for (int i = 1; found != nullptr && i < parts.size(); ++i) {
    QObject* next = nullptr;
    for (QObject* child : found->children()) {
      if (child != nullptr && NameInPath(child) == parts.at(i)) {
        next = child;
        break;
      }
    }
    found = next;
  }
  return found;
}

// 引导只能高亮 QWidget（引擎的目标类型只有 控件 / 矩形 / 图），
// 所以菜单项那种 QAction 到这里会被挡掉
QWidget* ResolveWidget(const QString& path) {
  return qobject_cast<QWidget*>(FindObjectByPath(path));
}

}  // namespace

TourBridge::TourBridge(TourOverlay* overlay, QWidget* parent)
    : QWidget(parent), overlay_(overlay) {
  // show() 不能省：funq 客户端等控件 visible 才返回。0 尺寸 + 不挡鼠标，
  // 用户看不见它——它只是 funq 的落脚点（objectName 是脚本的寻址锚点）
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
  // 多靶：`paths` 是数组，[0] 是锚点、其余是额外靶。与单数 `path` 同时给时
  // 按 paths 处理并记一条警告——混着写多半是笔误，静默挑一个会让人查半天
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
    result["ok"] = false;
    result["message"] =
        QStringLiteral("按路径找不到控件（路径要写成 funq 那种完整路径）: %1")
            .arg(path);
    qWarning() << "引导桥接：按路径找不到控件" << path;
    return result;
  }

  // 不可见的控件几何是未初始化的（从没显示过的 QMenu 报 0,0），框会跑到屏幕
  // 角落——宁可报失败，也不要画一个假的框
  if (!target->isVisible() || target->size().isEmpty()) {
    result["ok"] = false;
    result["message"] =
        QStringLiteral("目标不可见或尺寸为空，不给高亮: %1").arg(path);
    return result;
  }

  // 额外靶与锚点同一套校验，外加**必须同窗口**：跨窗口的靶会落在遮罩之外，
  // 屏幕上看不见框而这里照样报成功
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
  // `tab` 指页签条第 N 个。**页签不是 QWidget**（只是画出来的），按 path 找
  // 不到，只能用 tabRect() 算出矩形、走按矩形高亮那条路
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
    // tab 只作用于锚点；给了 tab 时额外靶仍是控件，由 extras 收
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
    // 不预先转成矩形：整窗高亮要靠 showHighlight 里的 setHost(inflate)，
    // 先转矩形就把那条绕过去了
    overlay_->showHighlight(target, title, text, extras);
  }

  // 视觉倒计时（秒）：只显示、**不推进任何东西**，下一步由脚本决定
  const int countdown = args.value(QStringLiteral("countdown"), 0).toInt();
  if (countdown > 0) {
    overlay_->showCountdown(countdown);
  }

  result["ok"] = true;
  result["message"] = QStringLiteral("高亮 %1").arg(path);
  // 把算出来的数字一并回报：出问题时只能靠这个定位。`rect` 只报锚点，
  // 框数另给 `count`
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
