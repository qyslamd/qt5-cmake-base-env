#include "buttons_page.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QToolButton>

#include "demo_helpers.h"

ButtonsPage::ButtonsPage(QWidget* parent) : QWidget(parent) { initUi(); }

void ButtonsPage::initUi() {
  auto* grid = new QGridLayout(this);
  grid->setContentsMargins(16, 16, 16, 16);
  grid->setSpacing(12);

  {
    auto* layout = new QHBoxLayout();
    layout->addWidget(new QPushButton(QStringLiteral("默认（主按钮）")));

    auto* secondary = new QPushButton(QStringLiteral("次按钮"));
    secondary->setProperty("role", QStringLiteral("secondary"));
    layout->addWidget(secondary);

    auto* danger = new QPushButton(QStringLiteral("危险操作"));
    danger->setProperty("role", QStringLiteral("danger"));
    layout->addWidget(danger);

    auto* disabled = new QPushButton(QStringLiteral("禁用"));
    disabled->setEnabled(false);
    layout->addWidget(disabled);

    layout->addStretch();
    grid->addWidget(
        MakeGroup(QStringLiteral("QPushButton：默认 / role=\"secondary\" / "
                                 "role=\"danger\" / disabled"),
                  layout),
        0, 0);
  }

  {
    auto* layout = new QHBoxLayout();

    auto* neutral = new QToolButton();
    neutral->setText(QStringLiteral("中性"));
    layout->addWidget(neutral);

    auto* primary = new QToolButton();
    primary->setText(QStringLiteral("主操作"));
    primary->setProperty("role", QStringLiteral("primary"));
    layout->addWidget(primary);

    auto* danger = new QToolButton();
    danger->setText(QStringLiteral("删除"));
    danger->setProperty("role", QStringLiteral("danger"));
    layout->addWidget(danger);

    auto* checked = new QToolButton();
    checked->setText(QStringLiteral("已选中"));
    checked->setCheckable(true);
    checked->setChecked(true);
    layout->addWidget(checked);

    layout->addStretch();
    grid->addWidget(
        MakeGroup(QStringLiteral("QToolButton：中性 / role=\"primary\" / "
                                 "role=\"danger\" / checked"),
                  layout),
        1, 0);
  }

  {
    auto* layout = new QGridLayout();
    layout->addWidget(new QCheckBox(QStringLiteral("未选中")), 0, 0);

    auto* check2 = new QCheckBox(QStringLiteral("已选中"));
    check2->setChecked(true);
    layout->addWidget(check2, 0, 1);

    auto* check3 = new QCheckBox(QStringLiteral("禁用"));
    check3->setEnabled(false);
    layout->addWidget(check3, 0, 2);

    auto* radio1 = new QRadioButton(QStringLiteral("选项 A"));
    radio1->setChecked(true);
    layout->addWidget(radio1, 1, 0);

    layout->addWidget(new QRadioButton(QStringLiteral("选项 B")), 1, 1);

    auto* radio3 = new QRadioButton(QStringLiteral("禁用"));
    radio3->setEnabled(false);
    layout->addWidget(radio3, 1, 2);

    grid->addWidget(
        MakeGroup(QStringLiteral("QCheckBox / QRadioButton"), layout), 2, 0);
  }

  {
    auto* layout = new QHBoxLayout();
    auto* dbb =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                             QDialogButtonBox::Help);
    layout->addWidget(dbb);
    layout->addStretch();
    grid->addWidget(
        MakeGroup(QStringLiteral("QDialogButtonBox：按钮统一 min-width: 80px"),
                  layout),
        3, 0);
  }

  grid->setRowStretch(4, 1);
}