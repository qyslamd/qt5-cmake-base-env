#include "demo_topics.h"

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <Qt>

#include <QWidget>

#include <tour_catalog.h>
#include <tour_controller.h>

namespace {

// 按 objectName 找控件。找不到返回 nullptr——TourStep 接受空目标，那一步
// 会退化成纯讲解，不会崩
QWidget* Find(QWidget* root, const QString& name) {
  return root->findChild<QWidget*>(name);
}

TourStep* MakeStep(QWidget* target, const QString& title,
                       const QString& description) {
  auto* step = new TourStep(target);
  step->setTitle(title);
  step->setDescription(description);
  return step;
}

// 运行时画一张示意图给 QPixmap 目标用：示例不该依赖额外的图片资源
QPixmap MakeSamplePixmap() {
  QPixmap pixmap(420, 200);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  const QColor accent(0, 122, 204);
  painter.setBrush(QColor(accent.red(), accent.green(), accent.blue(), 40));
  painter.setPen(QPen(accent, 2, Qt::DashLine));
  painter.drawRoundedRect(pixmap.rect().adjusted(1, 1, -1, -1), 12, 12);

  painter.setPen(accent);
  painter.drawText(pixmap.rect(), Qt::AlignCenter,
                   QStringLiteral("这里放你的示意图\nQPixmap 目标会居中显示"));
  return pixmap;
}

}  // namespace

void AddDemoTopics(TourController* controller, QWidget* root) {
  if (controller == nullptr || root == nullptr) {
    return;
  }

  // ------------------------------------------------------------------
  // 一、基本控件导览（讲解式）：逐个控件高亮
  // ------------------------------------------------------------------
  auto* controls = new GuideTopic(QStringLiteral("基本控件导览"),
                                    QStringLiteral("认识窗口里的几个控件"));
  controls->addStep(MakeStep(
      Find(root, QStringLiteral("demoNameEdit")), QStringLiteral("输入框"),
      QStringLiteral("在这里填名称。高亮框圈的是控件本身，气泡会自动挑一侧"
                     "贴过去。")));
  controls->addStep(MakeStep(
      Find(root, QStringLiteral("demoModeCombo")), QStringLiteral("下拉框"),
      QStringLiteral("选运行模式。讲解式主题按倒计时往下走，"
                     "也可以自己点「下一步」或「上一步」。")));
  controls->addStep(MakeStep(
      Find(root, QStringLiteral("demoVerboseCheck")), QStringLiteral("开关"),
      QStringLiteral("勾上会输出更详细的日志。")));
  controls->addStep(MakeStep(
      Find(root, QStringLiteral("demoApplyButton")), QStringLiteral("应用"),
      QStringLiteral("把上面的设置应用下去。")));
  // 收尾步：空目标 = 纯讲解，气泡居中、不指任何控件
  controls->addStep(MakeStep(nullptr, QStringLiteral("这条就到这里"),
                             QStringLiteral("下一条是实操式的——"
                                            "那条不自动翻页，等你自己动手。")));
  controller->addTopic(controls);

  // ------------------------------------------------------------------
  // 二、动手试试（实操式）：遮罩穿透，边看边操作
  // ------------------------------------------------------------------
  auto* hands_on = new GuideTopic(QStringLiteral("动手试试"),
                                    QStringLiteral("遮罩穿透，边看边操作"));
  hands_on->setInteractive(true);
  hands_on->addStep(MakeStep(
      Find(root, QStringLiteral("demoNameEdit")), QStringLiteral("先填个名字"),
      QStringLiteral("此刻遮罩对鼠标穿透，你可以直接点、直接输入——"
                     "被高亮的控件照常响应。")));
  hands_on->addStep(MakeStep(
      Find(root, QStringLiteral("demoApplyButton")),
      QStringLiteral("再点一下「应用」"),
      QStringLiteral("实操式主题不自动翻页：等用户动手优先于自动跳步")));
  hands_on->addStep(MakeStep(
      nullptr, QStringLiteral("手也动完了"),
      QStringLiteral("退出走气泡的「跳过」——实操模式下点遮罩空白不退出，"
                     "那片空白特意让给下层控件了。")));
  controller->addTopic(hands_on);

  // ------------------------------------------------------------------
  // 三、区域和插图：QRect 与 QPixmap 两种目标
  // ------------------------------------------------------------------
  auto* shapes = new GuideTopic(QStringLiteral("区域和插图"),
                                  QStringLiteral("高亮一块区域，或插一张图"));

  // QRect 目标必须是**全局坐标**，而窗口位置要到运行时才知道，所以目标的计算
  // 放进 enter 回调——这正是 TourStep::setTarget 的用途
  QWidget* status = Find(root, QStringLiteral("demoStatusLabel"));
  auto* rect_step =
      MakeStep(nullptr, QStringLiteral("框住一块区域"),
               QStringLiteral("这个框是 QRect 目标：它不是哪个控件，"
                              "而是屏幕上一块区域（坐标运行时现算）。"));
  if (status != nullptr) {
    rect_step->setEnterFunc([rect_step, status]() {
      // 比控件本身往外扩一点，看得出是"区域"而不是"控件边界"
      const QPoint top_left = status->mapToGlobal(QPoint(-8, -6));
      rect_step->setTarget(QRect(top_left, status->size() + QSize(16, 12)));
    });
  }
  shapes->addStep(rect_step);

  auto* pixmap_step = MakeStep(
      nullptr, QStringLiteral("插一张示意图"),
      QStringLiteral("QPixmap 目标居中显示，高亮框正好包住它。"
                     "图是运行时用 QPainter 画的，示例不依赖图片资源。"));
  pixmap_step->setTarget(MakeSamplePixmap());
  shapes->addStep(pixmap_step);

  shapes->addStep(MakeStep(nullptr, QStringLiteral("演示结束"),
                           QStringLiteral("三条主题都走完了。"
                                          "主页里可以单独挑一条重播。")));
  controller->addTopic(shapes);
}
