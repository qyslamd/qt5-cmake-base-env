#include "gallery_window.h"

#include <functional>
#include <vector>

#include <QColor>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "buttons/switch_button.h"
#include "log_output_panel.h"
#include "logger/logger.h"
#include "progress_bars/circle_progress_bar.h"
#include "progress_bars/dot_circle_progress_bar.h"
#include "progress_bars/hourglass_progress_bar.h"
#include "progress_bars/water_progress_bar.h"
#include "radial_navigator.h"
#include "sector_menu.h"
#include "splash_screen.h"
#include "tab_appearance/battery_tab_appearance.h"
#include "tab_appearance/cat_ear_tab_appearance.h"
#include "tab_appearance/chrome_tab_appearance.h"
#include "tab_appearance/firefox_tab_appearance.h"
#include "tab_appearance/flame_tab_appearance.h"
#include "tab_appearance/jelly_tab_appearance.h"
#include "tab_appearance/liquid_tab_appearance.h"
#include "tab_appearance/neon_tab_appearance.h"
#include "tab_appearance/tab_bar_style.h"
#include "tab_appearance/torn_paper_tab_appearance.h"

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

// 给进度条加一行类名标注：四种风格并排摆，不标出来分不清谁是谁
QWidget* MakeProgressCell(QWidget* parent, const QString& title, QWidget* bar) {
  auto* cell = new QWidget(parent);
  auto* layout = new QVBoxLayout(cell);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  auto* label = new QLabel(title, cell);
  label->setAlignment(Qt::AlignCenter);
  layout->addWidget(label);
  layout->addWidget(bar, 1);
  return cell;
}

// 切换表的一行：外观名、构造外观的工厂、以及该外观的主题微调。
// 微调多半不能省：各外观自带的填充深浅差别很大，用同一套文字色会在浅色
// 渐变上糊成一片；填充本身就是深色的外观才沿用默认配色，此时 tune_theme
// 留空即可
struct AppearanceEntry {
  QString name;
  std::function<TabAppearance*()> create;
  std::function<void(TabTheme&)> tune_theme;
};

// 演示用的外观清单。每项都返回新对象，因为 TabBarStyle 换外观时
// 会接管并销毁旧的那一个
const std::vector<AppearanceEntry>& AppearanceTable() {
  static const std::vector<AppearanceEntry> kTable = {
      {QStringLiteral("Chrome"),
       []() -> TabAppearance* { return new ChromeTabAppearance(); },
       [](TabTheme& theme) {
         // 取应用强调色，和 default.qss 里的蓝主题一致
         theme.selectedBackground = QColor(0x00, 0x7A, 0xCC);
         theme.selectedTextColor = Qt::white;
       }},
      {QStringLiteral("Firefox"),
       []() -> TabAppearance* { return new FirefoxTabAppearance(); },
       [](TabTheme& theme) { theme.selectedTextColor = Qt::white; }},
      {QStringLiteral("Liquid"),
       []() -> TabAppearance* { return new LiquidTabAppearance(); },
       [](TabTheme& theme) {
         // 蓝青渐变偏亮，白字对比度不足，改深蓝
         theme.selectedTextColor = QColor(0x08, 0x3B, 0x66);
       }},
      {QStringLiteral("CatEar"),
       []() -> TabAppearance* { return new CatEarTabAppearance(); },
       [](TabTheme& theme) {
         theme.selectedTextColor = QColor(0x5A, 0x2E, 0x00);
       }},
      {QStringLiteral("Jelly"),
       []() -> TabAppearance* { return new JellyTabAppearance(); },
       [](TabTheme& theme) {
         theme.selectedTextColor = QColor(0x6B, 0x14, 0x3A);
       }},
      {QStringLiteral("TornPaper"),
       []() -> TabAppearance* { return new TornPaperTabAppearance(); },
       [](TabTheme& theme) {
         theme.selectedTextColor = QColor(0x4A, 0x3B, 0x1A);
       }},
      {QStringLiteral("Flame"),
       []() -> TabAppearance* { return new FlameTabAppearance(); },
       [](TabTheme& theme) {
         theme.selectedTextColor = QColor(0x4A, 0x3B, 0x1A);
       }},
      {QStringLiteral("Neon"),
       []() -> TabAppearance* { return new NeonTabAppearance(); },
       [](TabTheme& theme) {
         // 霓虹把整条标签铺成深色，未选中态也必须用浅色字
         theme.normalTextColor = QColor(0xC8, 0xE8, 0xFF);
         theme.selectedTextColor = Qt::white;
       }},
      {QStringLiteral("Battery"),
       []() -> TabAppearance* { return new BatteryTabAppearance(); },
       // 电池三态都是饱和色，明度 220，黑色文字在红黄绿上都够对比度，
       // 故无需微调
       {}},
  };
  return kTable;
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
  actions->addWidget(new QLabel(QStringLiteral("标签栏外观："), this));

  appearance_combo_ = new QComboBox(this);
  for (const auto& entry : AppearanceTable()) {
    appearance_combo_->addItem(entry.name);
  }
  actions->addWidget(appearance_combo_);
  root->addLayout(actions);

  // 上半部按部件分页；部件多了只加标签页，布局不用动。
  // 标签栏外观由顶部的下拉框切换，主标签栏本身就是演示对象
  tabs_ = new QTabWidget(this);
  tab_style_ = new TabBarStyle(tabs_->tabBar());
  tabs_->tabBar()->setStyle(tab_style_);

  tabs_->addTab(createSwitchButtonPage(), QStringLiteral("SwitchButton"));
  tabs_->addTab(createSectorMenuPage(), QStringLiteral("SectorMenu"));
  tabs_->addTab(createRadialNavigatorPage(), QStringLiteral("RadialNavigator"));
  tabs_->addTab(createProgressBarPage(), QStringLiteral("ProgressBar"));

  applyAppearance(appearance_combo_->currentIndex());

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

QWidget* GalleryWindow::createProgressBarPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  layout->addWidget(MakeHint(
      page,
      QStringLiteral("自绘进度条族。ProgressBase 提供 value / minimum / "
                     "maximum / textVisible 四个属性与归一化进度 ratio()，"
                     "四种风格各自实现 paintEvent()：圆环、圆点拖尾、沙漏、"
                     "水球。下方滑块同时驱动四种，其中水球的波浪另有定时器"
                     "自走，不受进度影响。")));

  circle_progress_ = new CircleProgress(page);
  dot_progress_ = new DotCircleProgress(page);
  hourglass_progress_ = new HourglassProgress(page);
  water_progress_ = new WaterProgress(page);

  auto* grid = new QGridLayout();
  grid->setSpacing(12);
  grid->addWidget(MakeProgressCell(page, QStringLiteral("CircleProgress"),
                                   circle_progress_),
                  0, 0);
  grid->addWidget(MakeProgressCell(page, QStringLiteral("DotCircleProgress"),
                                   dot_progress_),
                  0, 1);
  grid->addWidget(MakeProgressCell(page, QStringLiteral("HourglassProgress"),
                                   hourglass_progress_),
                  1, 0);
  grid->addWidget(
      MakeProgressCell(page, QStringLiteral("WaterProgress"), water_progress_),
      1, 1);
  layout->addLayout(grid, 1);

  progress_slider_ = new QSlider(Qt::Horizontal, page);
  progress_slider_->setRange(0, 100);
  // 初值取中间偏一点，四种风格一眼能看出差别；0 时水球和圆点几乎是空的
  progress_slider_->setValue(35);
  layout->addWidget(progress_slider_);

  applyProgress(progress_slider_->value());
  return page;
}

void GalleryWindow::applyProgress(int value) {
  circle_progress_->setValue(value);
  dot_progress_->setValue(value);
  hourglass_progress_->setValue(value);
  water_progress_->setValue(value);
}

void GalleryWindow::applyAppearance(int index) {
  const auto& table = AppearanceTable();
  if (index < 0 || index >= static_cast<int>(table.size())) {
    return;
  }

  const AppearanceEntry& entry = table[index];
  // 先回到默认主题再套微调，否则上一个外观留下的颜色会带到下一个
  tab_style_->theme() = TabTheme();
  if (entry.tune_theme) {
    entry.tune_theme(tab_style_->theme());
  }
  tab_style_->setAppearance(entry.create());

  LOG_INFO("GALLERY", "标签栏外观切换为：{}", entry.name.toStdString());
}

void GalleryWindow::initSignals() {
  connect(switch_button_, &SwitchButton::toggled, this, [](bool checked) {
    LOG_INFO("GALLERY", "SwitchButton 切换为：{}", checked ? "开" : "关");
  });

  connect(tabs_, &QTabWidget::currentChanged, tabs_, [this](int index) {
    LOG_INFO("GALLERY", "主标签页切换：{}",
             tabs_->tabText(index).toStdString());
  });

  connect(appearance_combo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &GalleryWindow::applyAppearance);

  connect(progress_slider_, &QSlider::valueChanged, this,
          &GalleryWindow::applyProgress);
  // 拖动过程中每变一格就记一条会把日志面板淹掉，松手时记最终值即可
  connect(progress_slider_, &QSlider::sliderReleased, this, [this]() {
    LOG_INFO("GALLERY", "进度条演示拖动到：{}%", progress_slider_->value());
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
