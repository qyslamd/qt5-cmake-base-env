#include "gallery_window.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "radial_navigator.h"
#include "sector_menu.h"
#include "splash_screen.h"
#include "switch_button.h"

namespace {

// SplashScreen 演示的推进节奏与步数，仅用于让效果可见
constexpr int kSplashStepMs = 600;
constexpr int kSplashSteps = 4;

}  // namespace

GalleryWindow::GalleryWindow(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();
}

void GalleryWindow::initUi() {
  setWindowTitle(QStringLiteral("libui 部件示例"));
  resize(900, 560);

  auto* root = new QHBoxLayout(this);

  // 左侧：各部件依次摆放
  auto* left = new QVBoxLayout();
  left->setSpacing(12);

  left->addWidget(new QLabel(QStringLiteral("SwitchButton 滑块开关"), this));
  switch_button_ = new SwitchButton(this);
  switch_button_->setChecked(true);
  left->addWidget(switch_button_);

  left->addWidget(new QLabel(QStringLiteral("SectorMenu 环形扇区菜单"), this));
  sector_menu_ = new SectorMenu(this);
  sector_menu_->setRadiusRatio(0.95, 0.55);
  // SectorMenu 的动作用回调表达，与 RadialNavigator 一致，故在建立条目时
  // 一并给出；QObject 派生的部件（SwitchButton）才走 initSignals 连接。
  for (int i = 1; i <= 5; ++i) {
    sector_menu_->addItem(
        QStringLiteral("扇区 %1").arg(i),
        [this, i]() {
          appendEvent(QStringLiteral("SectorMenu 触发：扇区 %1").arg(i));
        },
        QColor::fromHsv((i - 1) * 60, 140, 200));
  }
  left->addWidget(sector_menu_, 1);

  left->addWidget(
      new QLabel(QStringLiteral("RadialNavigator 环形导航"), this));
  navigator_ = new RadialNavigator(this);
  navigator_->setMinimumHeight(200);
  // 图标留空：本示例只演示交互与排布，实际项目按需传入各自图标
  for (int i = 1; i <= 6; ++i) {
    navigator_->addButton(
        QPixmap(), QStringLiteral("项 %1").arg(i),
        [this, i]() {
          appendEvent(QStringLiteral("RadialNavigator 触发：项 %1").arg(i));
        },
        QStringLiteral("第 %1 项").arg(i));
  }
  left->addWidget(navigator_, 1);

  splash_button_ = new QPushButton(QStringLiteral("显示 SplashScreen"), this);
  left->addWidget(splash_button_);

  // 右侧：事件输出，用于观察各部件是否按预期回调
  event_view_ = new QPlainTextEdit(this);
  event_view_->setReadOnly(true);
  event_view_->setObjectName(QStringLiteral("galleryEventView"));

  root->addLayout(left, 3);
  root->addWidget(event_view_, 2);

  splash_timer_ = new QTimer(this);
  splash_timer_->setInterval(kSplashStepMs);
}

void GalleryWindow::initSignals() {
  connect(switch_button_, &SwitchButton::toggled, this, [this](bool checked) {
    appendEvent(QStringLiteral("SwitchButton 切换为：%1")
                    .arg(checked ? QStringLiteral("开")
                                 : QStringLiteral("关")));
  });

  connect(splash_button_, &QPushButton::clicked, this,
          &GalleryWindow::onShowSplash);
  connect(splash_timer_, &QTimer::timeout, this, &GalleryWindow::onSplashStep);
}

void GalleryWindow::onShowSplash() {
  if (!splash_) {
    splash_ = new SplashScreen(this);
  }

  // 用法与 src/app/main.cpp 一致：显示后按步骤推进进度，完成后 finish
  splash_step_ = 0;
  splash_->setProgress(0);
  splash_->setStatusText(QStringLiteral("正在准备..."));
  splash_->show();
  splash_timer_->start();

  appendEvent(QStringLiteral("SplashScreen 已显示"));
}

void GalleryWindow::onSplashStep() {
  ++splash_step_;
  const int percent = splash_step_ * 100 / kSplashSteps;
  splash_->setProgress(percent);
  splash_->setStatusText(QStringLiteral("步骤 %1 / %2")
                             .arg(splash_step_)
                             .arg(kSplashSteps));

  if (splash_step_ >= kSplashSteps) {
    splash_timer_->stop();
    splash_->finish();
    appendEvent(QStringLiteral("SplashScreen 已结束"));
  }
}

void GalleryWindow::appendEvent(const QString& text) {
  event_view_->appendPlainText(
      QStringLiteral("[%1] %2")
          .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
          .arg(text));
}
