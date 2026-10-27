#include "container_page.h"

#include <QLabel>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

#include "demo_helpers.h"

ContainerPage::ContainerPage(QWidget* parent) : QWidget(parent) { initUi(); }

void ContainerPage::initUi() {
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(16, 16, 16, 16);
  outer->setSpacing(12);

  {
    auto* layout = new QVBoxLayout();
    layout->addWidget(new QLabel(QStringLiteral(
        "这是 QGroupBox 的内容区。鼠标移到边框上会看到强调色描边。")));
    outer->addWidget(MakeGroup(
        QStringLiteral("QGroupBox：标题在左上，hover 时边框变强调色"), layout));
  }

  {
    auto* layout = new QVBoxLayout();
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(
        new QLabel(QStringLiteral("左面板\n拖动中间的 handle 改变宽度")));
    splitter->addWidget(new QLabel(QStringLiteral("右面板")));
    splitter->setSizes({200, 400});
    splitter->setMinimumHeight(80);
    layout->addWidget(splitter);
    outer->addWidget(
        MakeGroup(QStringLiteral("QSplitter：handle 悬停变强调色"), layout));
  }

  {
    auto* layout = new QVBoxLayout();

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setMinimumHeight(220);

    auto* content = new QWidget();
    auto* content_layout = new QVBoxLayout(content);
    content_layout->setContentsMargins(8, 8, 8, 8);
    content_layout->setSpacing(8);
    for (int i = 0; i < 20; ++i) {
      content_layout->addWidget(new QLabel(
          QStringLiteral("第 %1 行：滚动区内的内容。viewport 已设为透明，"
                         "背景由 QScrollArea 提供。")
              .arg(i + 1)));
    }
    scroll->setWidget(content);

    layout->addWidget(scroll);
    outer->addWidget(MakeGroup(
        QStringLiteral("QScrollArea：白色底 + 8px 圆角边框，滚动条内收 1px"),
        layout));
  }

  outer->addStretch();
}