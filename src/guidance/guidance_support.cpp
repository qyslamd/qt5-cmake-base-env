#include "guidance_support.h"

#include <QAction>
#include <QApplication>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QString>
#include <QWidget>
#include <QtDebug>

#include "guidance_controller.h"
#include "guidance_homepage.h"
#include "guidance_presentation.h"

namespace {

// funq 的路径形如 "MainWindow::settingsDialog::settingsCloseButton"，取最后
// 一段当 objectName，用 Qt 的 findChild 找。
//
// 刻意不用 funq 的 ObjectPath：那会让 guidance 库链上 funq，为了一句高亮把整个
// 库绑上去不值得。代价是只认最后一段——重名控件会命中第一个
QWidget* ResolveWidget(const QString& path) {
  const int sep = path.lastIndexOf(QStringLiteral("::"));
  const QString name = sep >= 0 ? path.mid(sep + 2) : path;
  if (name.isEmpty()) {
    return nullptr;
  }

  // 控件可能在任何顶层窗口里（主窗口、设置对话框……），逐个找
  const QWidgetList tops = QApplication::topLevelWidgets();
  for (QWidget* top : tops) {
    if (top == nullptr) {
      continue;
    }
    if (top->objectName() == name) {
      return top;
    }
    if (auto* found = top->findChild<QWidget*>(name)) {
      return found;
    }
  }
  return nullptr;
}

}  // namespace

GuidanceSupport::GuidanceSupport(GuidanceController* controller,
                                 QWidget* parent)
    : QWidget(parent),
      controller_(controller),
      presentation_(controller != nullptr ? controller->presentation()
                                          : nullptr) {
  // funq 靠 objectName 定位到本对象再 call_slot。
  //
  // 做成独立顶层窗口而不是挂在主窗口下：脚本第一步（登录）就跑在主窗口**还不
  // 存在**的时候，挂上去的话那几步没法高亮。Tool 标志让它不进任务栏，
  // 0 尺寸 + 不挡鼠标，用户看不见它——它只是 funq 的落脚点。
  // show() 不能省：funq 客户端默认等控件 visible 才返回，不 show 会一直超时
  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
  setObjectName(QStringLiteral("guidanceBridge"));
  resize(0, 0);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  show();
}

void GuidanceSupport::attachMenu(QMainWindow* window) {
  if (window == nullptr) {
    return;
  }

  QMenu* menu = window->menuBar()->addMenu(QStringLiteral("功能引导"));
  menu->setObjectName(QStringLiteral("menuGuidance"));

  QAction* home_action = menu->addAction(QStringLiteral("打开引导主页"));
  home_action->setObjectName(QStringLiteral("actionGuidanceHome"));
  connect(home_action, &QAction::triggered, this, [this, window]() {
    if (controller_ == nullptr) {
      return;
    }
    // 懒建一次后复用。父级给主窗口，随窗口销毁
    if (home_page_ == nullptr) {
      home_page_ = new GuidanceHomePage(controller_, window);
    }
    // exec() 是"显示 + 阻塞到关闭"：主页里点卡片会先隐藏自己（hideEvent 发
    // closed），局部事件循环随之退出，控制器才开播——所以这里阻塞不会挡住引导
    home_page_->exec();
  });

  // 直接播第一条主题的快捷入口，省掉主页那一步
  QAction* tour_action = menu->addAction(QStringLiteral("模板导览"));
  tour_action->setObjectName(QStringLiteral("actionTemplateTour"));
  connect(tour_action, &QAction::triggered, this, [this]() {
    if (controller_ == nullptr || controller_->flows().isEmpty()) {
      return;
    }
    controller_->startOne(controller_->flows().first(), /*autoMode=*/true);
  });
}

QVariant GuidanceSupport::showHighlight(const QVariant& params) {
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
    result["message"] = QStringLiteral("按路径找不到控件: %1").arg(path);
    qWarning() << "引导桥接：按路径找不到控件" << path;
    return result;
  }

  // 不可见的控件不给高亮：它的几何是未初始化的，画出来的框会跑到屏幕角落去。
  // 踩过——菜单栏里的 QMenu 从没显示过，报出来是 (0,0,100x30)，遮罩被铺到了
  // 屏幕左上角一个 124x54 的小方块上。宁可报失败，也不要画一个假的框
  if (!target->isVisible() || target->size().isEmpty()) {
    result["ok"] = false;
    result["message"] =
        QStringLiteral("目标不可见或尺寸为空，不给高亮: %1").arg(path);
    return result;
  }

  // 被动模式：只显示、不交互，推进全由外部脚本给（设计与取舍见
  // docs/plan/guidance接入funq.md）
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

QVariant GuidanceSupport::hideHighlight(const QVariant& params) {
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
