#include "guidance_bridge.h"

#include <QApplication>
#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtDebug>

#include <guidance_presentation.h>

namespace {

// 路径解析。规则**照 funq 的 ObjectPath**（libFunq/objectpath.cpp）重写：
//
//   名字   = objectName；为空时退到类名（所以路径里会有 "QWidget" 这种段）
//   唯一名 = 同名兄弟里非首个的带 "-<序号>" 后缀（settingsRow-1 就是这么来的）
//   路径   = 各段用 "::" 拼，解析时逐段下钻
//
// 自己写而不是把 funq 的源码拷进来：funq 是 CeCILL v2.1（copyleft），拷进
// src/guidance 会把我们自己的库一起卷进那个许可证。算法本身不受版权保护，
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

GuidanceBridge::GuidanceBridge(GuidancePresentation* presentation,
                               QWidget* parent)
    : QWidget(parent), presentation_(presentation) {
  // funq 靠 objectName 定位到本对象再 call_slot。Tool 标志让它不进任务栏，
  // 0 尺寸 + 不挡鼠标，用户看不见它——它只是 funq 的落脚点。
  // show() 不能省：客户端默认等控件 visible 才返回，不 show 会一直超时
  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
  setObjectName(QStringLiteral("guidanceBridge"));
  resize(0, 0);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  show();
}

QVariant GuidanceBridge::showHighlight(const QVariant& params) {
  QVariantMap result;
  if (presentation_ == nullptr) {
    result["ok"] = false;
    result["message"] = QStringLiteral("演示层尚未就绪");
    return result;
  }

  const QVariantMap args = params.toMap();
  const QString path = args.value(QStringLiteral("path")).toString();
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

  // 被动模式：只显示、不交互，推进全由外部脚本给
  presentation_->setPassiveMode(true);
  presentation_->showHighlight(target, args.value(QStringLiteral("title")).toString(),
                               args.value(QStringLiteral("text")).toString());

  result["ok"] = true;
  result["message"] = QStringLiteral("高亮 %1").arg(path);
  // 把算出来的数字一并回报：高亮/遮罩算在哪儿，出问题时只能靠这个定位
  const QRect rect = presentation_->highlightRect();
  const QRect overlay = presentation_->geometry();
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
  result["host"] = target->window() == nullptr
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

QVariant GuidanceBridge::hideHighlight(const QVariant& params) {
  Q_UNUSED(params);

  QVariantMap result;
  if (presentation_ == nullptr) {
    result["ok"] = false;
    result["message"] = QStringLiteral("演示层尚未就绪");
    return result;
  }
  presentation_->clearHighlight();
  result["ok"] = true;
  result["message"] = QStringLiteral("已清除高亮");
  return result;
}
