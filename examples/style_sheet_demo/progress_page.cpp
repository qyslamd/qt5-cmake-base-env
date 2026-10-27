#include "progress_page.h"

#include <QHBoxLayout>
#include <QProgressBar>
#include <QSlider>
#include <QVBoxLayout>

#include "demo_helpers.h"

ProgressPage::ProgressPage(QWidget* parent) : QWidget(parent) { initUi(); }

void ProgressPage::initUi() {
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(16, 16, 16, 16);
  outer->setSpacing(12);

  {
    auto* layout = new QVBoxLayout();

    auto* p0 = new QProgressBar();
    p0->setRange(0, 100);
    p0->setValue(0);
    layout->addWidget(p0);

    auto* p50 = new QProgressBar();
    p50->setRange(0, 100);
    p50->setValue(50);
    layout->addWidget(p50);

    auto* p100 = new QProgressBar();
    p100->setRange(0, 100);
    p100->setValue(100);
    layout->addWidget(p100);

    auto* p_disabled = new QProgressBar();
    p_disabled->setRange(0, 100);
    p_disabled->setValue(40);
    p_disabled->setEnabled(false);
    layout->addWidget(p_disabled);

    outer->addWidget(MakeGroup(
        QStringLiteral("QProgressBar：0% / 50% / 100% / disabled"), layout));
  }

  {
    auto* layout = new QHBoxLayout();

    auto* h = new QSlider(Qt::Horizontal);
    h->setRange(0, 100);
    h->setValue(35);
    layout->addWidget(h, 1);

    auto* v = new QSlider(Qt::Vertical);
    v->setRange(0, 100);
    v->setValue(60);
    v->setMinimumHeight(160);
    layout->addWidget(v);

    outer->addWidget(MakeGroup(QStringLiteral("QSlider：水平 / 垂直"), layout));
  }

  outer->addStretch();
}