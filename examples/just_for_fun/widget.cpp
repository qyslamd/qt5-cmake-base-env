#include "widget.h"

#include <QHBoxLayout>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

#include "alpine_dawn.h"
#include "ascii_dot_view.h"
#include "ascii_gl_view.h"
#include "big_text.h"
#include "desert_night.h"
#include "hourglass.h"
#include "marine_drive.h"
#include "ocean_sunset.h"
#include "rocket.h"
#include "text_view.h"
#include "train.h"
#include "tv_static.h"

// 半色调场景：一套 (step, color) 缓冲，两套 View
template <typename T>
static void addHalftoneScene(QTabWidget* tabGl, QTabWidget* tabDots,
                             const QString& name, const QColor& bg) {
  tabGl->addTab(new AsciiGLView<T>(bg.rgb()), name);
  tabDots->addTab(new AsciiDotView<T>(bg.rgb()), name);
}

// 字符场景：一套 (QChar, color) 缓冲，TextView 画
template <typename T>
static void addTextScene(QTabWidget* tabText, const QString& name,
                         const QColor& bg) {
  tabText->addTab(new TextView<T>(bg.rgb()), name);
}

Widget::Widget(QWidget* parent) : QWidget(parent) {
  resize(1202, 600);

  QTabWidget* tab_top = new QTabWidget(this);
  tab_top->setProperty("role", "viewSwitch");
  tab_top->tabBar()->setProperty("role", "viewSwitch");

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(tab_top);

  // ---- 半色调场景：OpenGL / QPainter 两层 ----
  QTabWidget* tabGl = new QTabWidget(this);
  tab_top->addTab(tabGl, QStringLiteral("OpenGL"));
  QTabWidget* tabDots = new QTabWidget(this);
  tab_top->addTab(tabDots, QStringLiteral("QPainter"));

  addHalftoneScene<MarineDrive>(tabGl, tabDots, QStringLiteral("MarineDrive"),
                                qRgb(0x07, 0x08, 0x0f));
  addHalftoneScene<AlpineDawn>(tabGl, tabDots, QStringLiteral("AlpineDawn"),
                               qRgb(0x09, 0x0c, 0x18));
  addHalftoneScene<DesertNight>(tabGl, tabDots, QStringLiteral("DesertNight"),
                                qRgb(0x04, 0x06, 0x0c));
  addHalftoneScene<OceanSunset>(tabGl, tabDots, QStringLiteral("OceanSunset"),
                                qRgb(0x0b, 0x08, 0x17));
  addHalftoneScene<BigText>(tabGl, tabDots, QStringLiteral("BigText"),
                            qRgb(0x0b, 0x08, 0x17));

  // ---- 字符场景：等宽字体逐格画 ----
  QTabWidget* tabText = new QTabWidget(this);
  tab_top->addTab(tabText, QStringLiteral("Text"));

  addTextScene<Rocket>(tabText, QStringLiteral("Rocket"),
                       qRgb(0x00, 0x00, 0x00));
  addTextScene<Hourglass>(tabText, QStringLiteral("Hourglass"),
                          qRgb(0x08, 0x08, 0x10));
  addTextScene<Train>(tabText, QStringLiteral("Train"), qRgb(0x0b, 0x08, 0x14));
  addTextScene<TvStatic>(tabText, QStringLiteral("TvStatic"),
                         qRgb(0x0b, 0x08, 0x14));
}

Widget::~Widget() = default;