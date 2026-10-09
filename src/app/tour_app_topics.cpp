#include "tour_app_topics.h"

#include <QMainWindow>
#include <QString>
#include <QWidget>

#include "tour_catalog.h"

namespace {

// 按 objectName 找控件。找不到返回 nullptr——TourStep 接受空目标，那一步
// 会退化成纯讲解（演示层会清掉上一步的高亮残留），不会崩
QWidget* FindWidget(QMainWindow* window, const QString& name) {
  return window->findChild<QWidget*>(name);
}

// 造一步并把文案填好。主题的链式构造器长在 new 表达式后面读着别扭，收口成
// 一个函数，写主题时只关心"指哪、说什么"
TourStep* MakeStep(QWidget* target, const QString& title,
                       const QString& description) {
  auto* step = new TourStep(target);
  step->setTitle(title);
  step->setDescription(description);
  return step;
}

}  // namespace

// 主题：认识这个模板的主界面。讲的都是模板真实存在的入口——中央占位区、
// 「工具」与「帮助」。这两处的容器随形态而变（功能区下是有名字的面板，菜单栏
// 下是菜单标题），而入口本身（日志窗口 / 设置 / 关于）是 QAction 不是控件、
// 高亮指不了，所以两种形态都指到容器上
TourTopic* BuildTemplateTourTopic(QMainWindow* window) {
  if (window == nullptr) {
    return nullptr;
  }

  auto* topic = new TourTopic(QStringLiteral("模板导览"),
                                QStringLiteral("认识这个模板的主界面"));

  topic->addStep(
      MakeStep(FindWidget(window, QStringLiteral("pagePlaceholderTitle")),
               QStringLiteral("业务区在这里"),
               QStringLiteral("中央这块是占位控件，替换成你自己的业务界面即可。"
                              "删掉它不影响上方入口、日志窗口与设置。")));

#ifdef APP_ENABLE_RIBBON
  const QString tools_target = QStringLiteral("ribbonPanelTool");
  const QString help_target = QStringLiteral("ribbonPanelHelp");
  const QString tools_where = QStringLiteral("这个面板");
#else
  const QString tools_target = QStringLiteral("menuTool");
  const QString help_target = QStringLiteral("menuHelp");
  const QString tools_where = QStringLiteral("这个菜单");
#endif

  const QString tools_desc =
      QStringLiteral("「日志窗口」看运行日志，带过滤、滚动锁定与清除；"
                     "「设置」改应用配置。两个入口都在%1下。")
          .arg(tools_where);

  topic->addStep(MakeStep(FindWidget(window, tools_target),
                         QStringLiteral("工具"), tools_desc));

  topic->addStep(MakeStep(FindWidget(window, help_target),
                         QStringLiteral("帮助"),
                         QStringLiteral("版本号与构建信息在「关于」里，"
                                        "排查问题时先看它。")));

  // 收尾步：空目标 = 纯讲解，气泡居中、不指任何控件
  topic->addStep(MakeStep(nullptr, QStringLiteral("就到这里"),
                         QStringLiteral("主界面自带的入口只有这些，"
                                        "其余由你替换进来。")));

  return topic;
}
