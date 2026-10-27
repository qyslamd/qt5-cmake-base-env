#include "misc_page.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QPen>
#include <QVBoxLayout>
#include <QVariant>

#include "demo_helpers.h"


MiscPage::MiscPage(QWidget* parent) : QWidget(parent) { initUi(); }

void MiscPage::initUi() {
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(16, 16, 16, 16);
  outer->setSpacing(12);

  {
    auto* layout = new QVBoxLayout();

    auto* view = new QGraphicsView();
    auto* scene = new QGraphicsScene(view);
    scene->setBackgroundBrush(QBrush(Qt::white));
    scene->addRect(20, 20, 200, 100, QPen(Qt::blue, 2), QBrush(Qt::lightGray));
    scene->addEllipse(260, 30, 100, 80, QPen(Qt::darkGreen, 2),
                      QBrush(QColor(200, 240, 200)));
    scene->addText(QStringLiteral("QGraphicsView 内容"),
                   QFont(QStringLiteral("Microsoft YaHei"), 12));
    view->setScene(scene);
    view->setMinimumHeight(220);

    layout->addWidget(view);
    outer->addWidget(MakeGroup(
        QStringLiteral(
            "QGraphicsView：透明底 + 圆角边框（widget.qss 里单独声明的）"),
        layout));
  }

  {
    auto* layout = new QVBoxLayout();

    layout->addWidget(new QLabel(QStringLiteral("默认文本：主色 #333333")));

    auto* error = new QLabel(QStringLiteral("错误文本：D13438，12px"));
    error->setProperty("role", QStringLiteral("error"));
    layout->addWidget(error);

    auto* hint = new QLabel(QStringLiteral("提示文本：999999，12px"));
    hint->setProperty("role", QStringLiteral("hint"));
    layout->addWidget(hint);

    auto* disabled = new QLabel(QStringLiteral("禁用文本"));
    disabled->setEnabled(false);
    layout->addWidget(disabled);

    outer->addWidget(MakeGroup(
        QStringLiteral(
            "QLabel：默认 / role=\"error\" / role=\"hint\" / disabled"),
        layout));
  }

  outer->addStretch();
}