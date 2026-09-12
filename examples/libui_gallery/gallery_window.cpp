#include "gallery_window.h"

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "log_output_panel.h"
#include "logger/logger.h"
#include "radial_navigator.h"
#include "sector_menu.h"
#include "splash_screen.h"
#include "switch_button.h"
#include "tab_appearance/chrome_tab_appearance.h"
#include "tab_appearance/firefox_tab_appearance.h"
#include "tab_appearance/tab_bar_style.h"

namespace {

// SplashScreen 演示的推进节奏与步数，仅用于让效果可见
constexpr int kSplashStepMs = 600;
constexpr int kSplashSteps = 4;

// 每页顶部的用法说明
QLabel* MakeHint(QWidget* parent, const QString& text) {
  auto* label = new QLabel(text, parent);
  label->setWordWrap(true);
  return label;
}

}  // namespace

GalleryWindow::GalleryWindow(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();
}

void GalleryWindow::initUi() {
  setWindowTitle(QStringLiteral("libui 部件演示"));
  resize(960, 720);

  auto* root = new QVBoxLayout(this);

  // 顶部：作用于整个示例的操作
  auto* actions = new QHBoxLayout();
  splash_button_ = new QPushButton(QStringLiteral("显示 SplashScreen"), this);
  actions->addWidget(splash_button_);
  actions->addStretch();
  root->addLayout(actions);

  // 上半部按部件分页；部件多了只加标签页，布局不用动。
  // 主标签栏本身用 Chrome 外观演示 TabBarStyle，主题色取应用强调色
  tabs_ = new QTabWidget(this);
  auto* chrome_style =
      new TabBarStyle(new ChromeTabAppearance(), tabs_->tabBar());
  chrome_style->theme().selectedBackground = QColor(0x00, 0x7A, 0xCC);
  tabs_->tabBar()->setStyle(chrome_style);

  tabs_->addTab(createSwitchButtonPage(), QStringLiteral("SwitchButton"));
  tabs_->addTab(createSectorMenuPage(), QStringLiteral("SectorMenu"));
  tabs_->addTab(createRadialNavigatorPage(), QStringLiteral("RadialNavigator"));
  tabs_->addTab(createTabStylePage(), QStringLiteral("TabBarStyle"));

  // 下半部为日志面板：部件的事件都经 LOG_INFO 落到这里
  log_panel_ = new LogOutputPanel(this);

  auto* splitter = new QSplitter(Qt::Vertical, this);
  splitter->addWidget(tabs_);
  splitter->addWidget(log_panel_);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);
  root->addWidget(splitter, 1);

  splash_timer_ = new QTimer(this);
  splash_timer_->setInterval(kSplashStepMs);
}

QWidget* GalleryWindow::createSwitchButtonPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  layout->addWidget(MakeHint(
      page,
      QStringLiteral(
          "滑块开关。勾选状态变化时发出 toggled 信号，属于 QAbstractButton "
          "派生类，因此用信号槽连接，而不是像下面两个部件那样给回调。")));

  switch_button_ = new SwitchButton(page);
  switch_button_->setChecked(true);
  layout->addWidget(switch_button_, 0, Qt::AlignLeft);
  layout->addStretch();
  return page;
}

QWidget* GalleryWindow::createSectorMenuPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  layout->addWidget(MakeHint(
      page, QStringLiteral(
                "环形扇区菜单。扇区由外弧、内弧与两条径向边构成；标题沿中弧"
                "排布，过宽时自动缩小字号。鼠标悬停高亮，点击触发对应扇区的"
                "回调。可调整半径比与起始角。")));

  auto* menu = new SectorMenu(page);
  menu->setRadiusRatio(0.95, 0.55);
  for (int i = 1; i <= 5; ++i) {
    const int index = i;
    menu->addItem(
        QStringLiteral("扇区 %1").arg(index),
        [index]() { LOG_INFO("GALLERY", "SectorMenu 触发：扇区 {}", index); },
        QColor::fromHsv((index - 1) * 60, 140, 200));
  }
  layout->addWidget(menu, 1);
  return page;
}

QWidget* GalleryWindow::createRadialNavigatorPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  layout->addWidget(MakeHint(
      page, QStringLiteral(
                "环形导航。中央开关展开收起，按钮带逐项延迟动画。图标本例留空，"
                "实际项目按需传入 QPixmap；点击执行按钮自带的回调，调用方无需"
                "维护索引映射。")));

  auto* navigator = new RadialNavigator(page);
  for (int i = 1; i <= 6; ++i) {
    const int index = i;
    navigator->addButton(
        QPixmap(), QStringLiteral("项 %1").arg(index),
        [index]() {
          LOG_INFO("GALLERY", "RadialNavigator 触发：项 {}", index);
        },
        QStringLiteral("第 %1 项").arg(index));
  }
  layout->addWidget(navigator, 1);
  return page;
}

QWidget* GalleryWindow::createTabStylePage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  layout->addWidget(MakeHint(
      page, QStringLiteral(
                "标签栏外观框架。TabBarStyle 是 QProxyStyle，把形状绘制委托"
                "给 TabAppearance，颜色与尺寸集中在 TabTheme：换外观不改"
                "配色，改配色不碰几何。本页是 Firefox 外观（默认紫主题），"
                "顶部主标签栏是 Chrome 外观（蓝主题），两者可对比。")));

  firefox_tabs_ = new QTabWidget(page);
  for (int i = 1; i <= 3; ++i) {
    auto* body =
        new QLabel(QStringLiteral("第 %1 页的内容。").arg(i), firefox_tabs_);
    body->setAlignment(Qt::AlignCenter);
    firefox_tabs_->addTab(body, QStringLiteral("页 %1").arg(i));
  }
  firefox_tabs_->tabBar()->setStyle(
      new TabBarStyle(new FirefoxTabAppearance(), firefox_tabs_->tabBar()));

  layout->addWidget(firefox_tabs_, 1);
  return page;
}

void GalleryWindow::initSignals() {
  connect(switch_button_, &SwitchButton::toggled, this, [](bool checked) {
    LOG_INFO("GALLERY", "SwitchButton 切换为：{}", checked ? "开" : "关");
  });

  connect(tabs_, &QTabWidget::currentChanged, tabs_, [this](int index) {
    LOG_INFO("GALLERY", "主标签页切换：{}",
             tabs_->tabText(index).toStdString());
  });

  connect(firefox_tabs_, &QTabWidget::currentChanged, firefox_tabs_,
          [this](int index) {
            LOG_INFO("GALLERY", "Firefox 风格标签页切换：{}",
                     firefox_tabs_->tabText(index).toStdString());
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

  LOG_INFO("GALLERY", "SplashScreen 已显示");
}

void GalleryWindow::onSplashStep() {
  ++splash_step_;
  splash_->setProgress(splash_step_ * 100 / kSplashSteps);
  splash_->setStatusText(
      QStringLiteral("步骤 %1 / %2").arg(splash_step_).arg(kSplashSteps));

  if (splash_step_ >= kSplashSteps) {
    splash_timer_->stop();
    splash_->finish();
    LOG_INFO("GALLERY", "SplashScreen 已结束");
  }
}
