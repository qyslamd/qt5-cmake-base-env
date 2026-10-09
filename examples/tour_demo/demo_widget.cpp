#include "demo_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

#include <tour_controller.h>
#include <tour_homepage.h>

#include "demo_topics.h"

DemoWidget::DemoWidget(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();

  // 引擎放在最后建：主题要靠 findChild 按 objectName 找到上面那些控件，
  // 控件没建齐就注册主题，高亮会全部退化成空目标
  controller_ = new TourController(this);
  controller_->setViewport(this);
  AddDemoTopics(controller_, this);
}

void DemoWidget::initUi() {
  setObjectName(QStringLiteral("demoWidget"));
  setWindowTitle(QStringLiteral("功能引导演示"));
  resize(720, 420);

  auto* layout = new QVBoxLayout(this);

  auto* title = new QLabel(QStringLiteral("功能引导演示"), this);
  title->setObjectName(QStringLiteral("demoTitle"));

  auto* hint =
      new QLabel(QStringLiteral("点「开始引导」打开主题主页。三条主题分别演示"
                                "控件高亮、实操式穿透、以及区域与插图目标。"),
                 this);
  hint->setObjectName(QStringLiteral("demoHint"));
  hint->setWordWrap(true);

  name_edit_ = new QLineEdit(this);
  name_edit_->setObjectName(QStringLiteral("demoNameEdit"));
  name_edit_->setPlaceholderText(QStringLiteral("名称"));

  mode_combo_ = new QComboBox(this);
  mode_combo_->setObjectName(QStringLiteral("demoModeCombo"));
  mode_combo_->addItem(QStringLiteral("标准"));
  mode_combo_->addItem(QStringLiteral("严格"));
  mode_combo_->addItem(QStringLiteral("快速"));

  verbose_check_ = new QCheckBox(QStringLiteral("输出详细日志"), this);
  verbose_check_->setObjectName(QStringLiteral("demoVerboseCheck"));

  apply_btn_ = new QPushButton(QStringLiteral("应用"), this);
  apply_btn_->setObjectName(QStringLiteral("demoApplyButton"));

  guide_btn_ = new QPushButton(QStringLiteral("开始引导"), this);
  guide_btn_->setObjectName(QStringLiteral("demoGuideButton"));

  status_label_ = new QLabel(QStringLiteral("就绪"), this);
  status_label_->setObjectName(QStringLiteral("demoStatusLabel"));

  auto* form = new QFormLayout();
  form->addRow(QStringLiteral("名称"), name_edit_);
  form->addRow(QStringLiteral("模式"), mode_combo_);
  form->addRow(QString(), verbose_check_);

  layout->addWidget(title);
  layout->addWidget(hint);
  layout->addSpacing(12);
  layout->addLayout(form);
  layout->addWidget(apply_btn_);
  layout->addStretch();
  layout->addWidget(guide_btn_);
  layout->addWidget(status_label_);
}

void DemoWidget::initSignals() {
  connect(apply_btn_, &QPushButton::clicked, this, [this]() {
    const QString name = name_edit_->text().isEmpty()
                             ? QStringLiteral("(未命名)")
                             : name_edit_->text();
    status_label_->setText(
        QStringLiteral("已应用：%1 / %2").arg(name, mode_combo_->currentText()));
  });
  connect(guide_btn_, &QPushButton::clicked, this,
          &DemoWidget::onOpenHomePage);
}

void DemoWidget::onOpenHomePage() {
  if (controller_ == nullptr) {
    return;
  }
  // 懒建一次后复用：主页是覆盖层，重复 new 会叠出一堆
  if (home_page_ == nullptr) {
    home_page_ = new TourHomePage(controller_, this);
  }
  // exec() 是"显示 + 阻塞到关闭"：主页里点卡片会先隐藏自己（hideEvent 发
  // closed），局部事件循环随之退出，控制器才开播——阻塞在这里不会挡住引导
  home_page_->exec();
}
