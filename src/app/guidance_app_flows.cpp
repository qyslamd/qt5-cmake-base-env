#include "guidance_app_flows.h"

#include <QMainWindow>
#include <QString>
#include <QWidget>

#include "guidance_config.h"

namespace {

// 按 objectName 找控件。找不到返回 nullptr——GuidanceStep 接受空目标，那一步
// 会退化成纯讲解（演示层会清掉上一步的高亮残留），不会崩
QWidget* FindWidget(QMainWindow* window, const QString& name) {
  return window->findChild<QWidget*>(name);
}

// 造一步并把文案填好。主题的链式构造器长在 new 表达式后面读着别扭，收口成
// 一个函数，写主题时只关心"指哪、说什么"
GuidanceStep* MakeStep(QWidget* target, const QString& title,
                       const QString& description) {
  auto* step = new GuidanceStep(target);
  step->setTitle(title);
  step->setDescription(description);
  return step;
}

}  // namespace

// 主题：认识这个模板的主界面。讲的都是模板真实存在的入口——中央占位区、
// 工具菜单、帮助菜单。菜单项（日志窗口 / 设置 / 关于）本身是 QAction 不是
// 控件，高亮指不了，所以指到它们所在的菜单标题上
GuidanceFlow* BuildTemplateTourFlow(QMainWindow* window) {
  if (window == nullptr) {
    return nullptr;
  }

  auto* flow = new GuidanceFlow(QStringLiteral("模板导览"),
                                QStringLiteral("认识这个模板的主界面"));

  flow->addStep(
      MakeStep(FindWidget(window, QStringLiteral("pagePlaceholderTitle")),
               QStringLiteral("业务区在这里"),
               QStringLiteral("中央这块是占位控件，替换成你自己的业务界面即可。"
                              "删掉它不影响菜单栏、日志窗口与设置。")));

  flow->addStep(MakeStep(FindWidget(window, QStringLiteral("menuTool")),
                         QStringLiteral("工具"),
                         QStringLiteral("「日志窗口」看运行日志，带过滤、"
                                        "滚动锁定与清除；「设置」改应用配置。"
                                        "两个入口都在这个菜单下。")));

  flow->addStep(MakeStep(FindWidget(window, QStringLiteral("menuHelp")),
                         QStringLiteral("帮助"),
                         QStringLiteral("版本号与构建信息在「关于」里，"
                                        "排查问题时先看它。")));

  // 收尾步：空目标 = 纯讲解，气泡居中、不指任何控件
  flow->addStep(MakeStep(nullptr, QStringLiteral("就到这里"),
                         QStringLiteral("主界面自带的入口只有这些，"
                                        "其余由你替换进来。")));

  return flow;
}
