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
#include "desert_night.h"
#include "marine_drive.h"

Widget::Widget(QWidget* parent) : QWidget(parent) {
  resize(1202, 600);

  QTabWidget* tab_top = new QTabWidget(this);
  tab_top->setProperty("role", "viewSwitch");
  tab_top->tabBar()->setProperty("role", "viewSwitch");

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(tab_top);

  QTabWidget* tab_gl = new QTabWidget(this);
  tab_top->addTab(tab_gl, QStringLiteral("OpenGL"));
  tab_gl->addTab(new AsciiGLView<MarineDrive>(qRgb(0x07, 0x08, 0x0f)),
                 QStringLiteral("MarineDrive"));
  tab_gl->addTab(new AsciiGLView<AlpineDawn>(qRgb(0x09, 0x0c, 0x18)),
                 QStringLiteral("AlpineDawn"));
  tab_gl->addTab(new AsciiGLView<DesertNight>(qRgb(0x04, 0x06, 0x0c)),
                 QStringLiteral("DesertNight"));

  QTabWidget* tab_dots = new QTabWidget(this);
  tab_top->addTab(tab_dots, QStringLiteral("QPainter"));
  tab_dots->addTab(new AsciiDotView<MarineDrive>(qRgb(0x07, 0x08, 0x0f)),
                   QStringLiteral("MarineDrive"));
  tab_dots->addTab(new AsciiDotView<AlpineDawn>(qRgb(0x09, 0x0c, 0x18)),
                   QStringLiteral("AlpineDawn"));
  tab_dots->addTab(new AsciiDotView<DesertNight>(qRgb(0x04, 0x06, 0x0c)),
                   QStringLiteral("DesertNight"));
}

Widget::~Widget() = default;