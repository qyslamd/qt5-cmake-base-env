#include "widget.h"

#include <QHBoxLayout>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

#include "alpine_dawn.h"
#include "ascii_dot_view.h"
#include "ascii_gl_view.h"
#include "big_text.h"
#include "desert_night.h"
#include "marine_drive.h"
#include "ocean_sunset.h"

template <typename T>
static void addScenePair(QTabWidget* tabGl, QTabWidget* tabDots,
                         const QString& name, const QColor& bg) {
  tabGl->addTab(new AsciiGLView<T>(bg.rgb()), name);
  tabDots->addTab(new AsciiDotView<T>(bg.rgb()), name);
}

Widget::Widget(QWidget* parent) : QWidget(parent) {
  resize(1202, 600);

  QTabWidget* tab_top = new QTabWidget(this);
  tab_top->setProperty("role", "viewSwitch");
  tab_top->tabBar()->setProperty("role", "viewSwitch");

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(tab_top);

  QTabWidget* tabGl = new QTabWidget(this);
  tab_top->addTab(tabGl, QStringLiteral("OpenGL"));
  QTabWidget* tabDots = new QTabWidget(this);
  tab_top->addTab(tabDots, QStringLiteral("QPainter"));

  //
  addScenePair<MarineDrive>(tabGl, tabDots, QStringLiteral("MarineDrive"),
                            qRgb(0x07, 0x08, 0x0f));
  addScenePair<AlpineDawn>(tabGl, tabDots, QStringLiteral("AlpineDawn"),
                           qRgb(0x09, 0x0c, 0x18));
  addScenePair<DesertNight>(tabGl, tabDots, QStringLiteral("DesertNight"),
                            qRgb(0x04, 0x06, 0x0c));
  addScenePair<OceanSunset>(tabGl, tabDots, QStringLiteral("OceanSunset"),
                            qRgb(0x0b, 0x08, 0x17));
  addScenePair<BigText>(tabGl, tabDots, QStringLiteral("BigText"),
                        qRgb(0x0b, 0x08, 0x17));
}

Widget::~Widget() = default;