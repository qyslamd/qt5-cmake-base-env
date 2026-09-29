#include "funq_target_window.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

// 第一页：输入类控件。改文本、换下拉项、勾选、调数值
QWidget* CreateInputPage(QLineEdit** name_edit, QComboBox** level_combo,
                         QCheckBox** agree_check, QSpinBox** amount_spin) {
  auto* page = new QWidget;
  page->setObjectName(QStringLiteral("input_page"));
  auto* form = new QFormLayout(page);

  *name_edit = new QLineEdit(page);
  (*name_edit)->setObjectName(QStringLiteral("name_edit"));
  form->addRow(QStringLiteral("名称"), *name_edit);

  *level_combo = new QComboBox(page);
  (*level_combo)->setObjectName(QStringLiteral("level_combo"));
  (*level_combo)->addItems({QStringLiteral("低"), QStringLiteral("中"),
                            QStringLiteral("高")});
  form->addRow(QStringLiteral("级别"), *level_combo);

  *amount_spin = new QSpinBox(page);
  (*amount_spin)->setObjectName(QStringLiteral("amount_spin"));
  (*amount_spin)->setRange(0, 100);
  form->addRow(QStringLiteral("数量"), *amount_spin);

  *agree_check = new QCheckBox(QStringLiteral("同意条款"), page);
  (*agree_check)->setObjectName(QStringLiteral("agree_check"));
  form->addRow(QString(), *agree_check);

  return page;
}

// 第二页：只读显示类控件。被第一页的输入驱动
QWidget* CreateDisplayPage(QLabel** status_label, QProgressBar** progress_bar,
                           QListWidget** item_list) {
  auto* page = new QWidget;
  page->setObjectName(QStringLiteral("display_page"));
  auto* layout = new QVBoxLayout(page);

  *status_label = new QLabel(QStringLiteral("（尚未输入）"), page);
  (*status_label)->setObjectName(QStringLiteral("status_label"));
  layout->addWidget(*status_label);

  *progress_bar = new QProgressBar(page);
  (*progress_bar)->setObjectName(QStringLiteral("progress_bar"));
  (*progress_bar)->setRange(0, 100);
  layout->addWidget(*progress_bar);

  *item_list = new QListWidget(page);
  (*item_list)->setObjectName(QStringLiteral("item_list"));
  (*item_list)->addItems({QStringLiteral("甲"), QStringLiteral("乙"),
                          QStringLiteral("丙")});
  layout->addWidget(*item_list);

  return page;
}

// 第三页：动作类控件。点击用例的目标
QWidget* CreateActionPage(QPushButton** submit_button,
                          QPushButton** reset_button, QLabel** click_label) {
  auto* page = new QWidget;
  page->setObjectName(QStringLiteral("action_page"));
  auto* layout = new QVBoxLayout(page);

  auto* row = new QHBoxLayout();
  *submit_button = new QPushButton(QStringLiteral("提交"), page);
  (*submit_button)->setObjectName(QStringLiteral("submit_button"));
  row->addWidget(*submit_button);

  *reset_button = new QPushButton(QStringLiteral("重置"), page);
  (*reset_button)->setObjectName(QStringLiteral("reset_button"));
  row->addWidget(*reset_button);
  layout->addLayout(row);

  *click_label = new QLabel(QStringLiteral("提交 0 次"), page);
  (*click_label)->setObjectName(QStringLiteral("click_label"));
  layout->addWidget(*click_label);

  layout->addStretch();
  return page;
}

}  // namespace

FunqTargetWindow::FunqTargetWindow(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();
}

void FunqTargetWindow::initUi() {
  setObjectName(QStringLiteral("FunqTarget"));
  setWindowTitle(QStringLiteral("funq 测试靶子"));
  resize(520, 360);

  auto* layout = new QVBoxLayout(this);

  tabs_ = new QTabWidget(this);
  tabs_->setObjectName(QStringLiteral("tabs"));

  QSpinBox* amount_spin = nullptr;
  QListWidget* item_list = nullptr;
  tabs_->addTab(CreateInputPage(&name_edit_, &level_combo_, &agree_check_,
                                &amount_spin),
                QStringLiteral("输入"));
  tabs_->addTab(CreateDisplayPage(&status_label_, &progress_bar_, &item_list),
                QStringLiteral("显示"));
  tabs_->addTab(
      CreateActionPage(&submit_button_, &reset_button_, &click_label_),
      QStringLiteral("动作"));

  // 数量微调框驱动进度条：又一条跨控件联动，且是数值型的
  connect(amount_spin, QOverload<int>::of(&QSpinBox::valueChanged),
          progress_bar_, &QProgressBar::setValue);

  layout->addWidget(tabs_);
}

void FunqTargetWindow::initSignals() {
  connect(name_edit_, &QLineEdit::textChanged, this,
          &FunqTargetWindow::onNameChanged);
  connect(submit_button_, &QPushButton::clicked, this,
          &FunqTargetWindow::onSubmitClicked);
  connect(reset_button_, &QPushButton::clicked, this,
          &FunqTargetWindow::onResetClicked);
}

void FunqTargetWindow::onNameChanged(const QString& text) {
  status_label_->setText(text.isEmpty()
                             ? QStringLiteral("（尚未输入）")
                             : QStringLiteral("当前名称：%1").arg(text));
}

void FunqTargetWindow::onSubmitClicked() {
  ++submit_count_;
  click_label_->setText(QStringLiteral("提交 %1 次").arg(submit_count_));
}

void FunqTargetWindow::onResetClicked() {
  name_edit_->clear();
  level_combo_->setCurrentIndex(0);
  agree_check_->setChecked(false);
  progress_bar_->setValue(0);
  submit_count_ = 0;
  click_label_->setText(QStringLiteral("提交 0 次"));
}
