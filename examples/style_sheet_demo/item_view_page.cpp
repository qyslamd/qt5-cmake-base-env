#include "item_view_page.h"

#include <QHeaderView>
#include <QListWidget>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

ItemViewPage::ItemViewPage(QWidget* parent) : QWidget(parent) { initUi(); }

void ItemViewPage::initUi() {
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(16, 16, 16, 16);
  outer->setSpacing(12);

  {
    auto* splitter = new QSplitter(Qt::Horizontal);

    auto* list = new QListWidget();
    for (int i = 0; i < 12; ++i) {
      list->addItem(QStringLiteral("列表项 %1").arg(i + 1));
    }
    list->setCurrentRow(2);
    splitter->addWidget(list);

    auto* tree = new QTreeWidget();
    tree->setHeaderLabels({QStringLiteral("名称"), QStringLiteral("值")});
    for (int i = 0; i < 3; ++i) {
      auto* top = new QTreeWidgetItem(tree);
      top->setText(0, QStringLiteral("分组 %1").arg(i + 1));
      for (int j = 0; j < 3; ++j) {
        auto* child = new QTreeWidgetItem(top);
        child->setText(0, QStringLiteral("子项 %1-%2").arg(i + 1).arg(j + 1));
        child->setText(1, QString::number((i + 1) * 10 + j));
      }
    }
    tree->expandAll();
    splitter->addWidget(tree);

    splitter->setSizes({300, 400});
    outer->addWidget(splitter, 1);
  }

  {
    auto* table = new QTableWidget(4, 3);
    table->setHorizontalHeaderLabels({QStringLiteral("设备"),
                                      QStringLiteral("状态"),
                                      QStringLiteral("读数")});

    const QStringList devices = {
        QStringLiteral("DMM-01"), QStringLiteral("DMM-02"),
        QStringLiteral("Source-01"), QStringLiteral("Source-02")};
    const QStringList states = {QStringLiteral("就绪"), QStringLiteral("采集"),
                                QStringLiteral("离线"), QStringLiteral("就绪")};
    const QStringList values = {QStringLiteral("1.234 V"),
                                QStringLiteral("0.987 A"), QStringLiteral("--"),
                                QStringLiteral("5.000 V")};

    for (int r = 0; r < 4; ++r) {
      table->setItem(r, 0, new QTableWidgetItem(devices.at(r)));
      table->setItem(r, 1, new QTableWidgetItem(states.at(r)));
      table->setItem(r, 2, new QTableWidgetItem(values.at(r)));
    }
    table->setCurrentCell(1, 0);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    outer->addWidget(table, 1);
  }
}