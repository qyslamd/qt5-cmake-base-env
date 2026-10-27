#include "input_page.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QTime>
#include <QTimeEdit>
#include <QVBoxLayout>

#include "demo_helpers.h"

InputPage::InputPage(QWidget* parent) : QWidget(parent) { initUi(); }

void InputPage::initUi() {
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(16, 16, 16, 16);
  outer->setSpacing(12);

  {
    auto* form = new QFormLayout();
    form->addRow(QStringLiteral("默认"),
                 new QLineEdit(QStringLiteral("hello")));

    auto* with_placeholder = new QLineEdit();
    with_placeholder->setPlaceholderText(QStringLiteral("请输入用户名…"));
    form->addRow(QStringLiteral("placeholder"), with_placeholder);

    auto* form_edit = new QLineEdit(QStringLiteral("表单里的输入框"));
    form_edit->setProperty("role", QStringLiteral("form"));
    form->addRow(QStringLiteral("role=\"form\""), form_edit);

    auto* error_edit = new QLineEdit(QStringLiteral("这个字段有错误"));
    error_edit->setProperty("error", QStringLiteral("true"));
    form->addRow(QStringLiteral("error=\"true\""), error_edit);

    auto* disabled_edit = new QLineEdit(QStringLiteral("禁用"));
    disabled_edit->setEnabled(false);
    form->addRow(QStringLiteral("disabled"), disabled_edit);

    outer->addWidget(MakeGroup(
        QStringLiteral("QLineEdit：默认 / placeholder / role=\"form\" / "
                       "error=\"true\" / disabled"),
        form));
  }

  {
    auto* layout = new QHBoxLayout();

    auto* te = new QTextEdit();
    te->setPlainText(
        QStringLiteral("QTextEdit：这里是示例文本。\n"
                       "边框是 none，只有 focus 时才会出现强调色边框。"));
    te->setMinimumHeight(100);
    layout->addWidget(te);

    auto* pe = new QPlainTextEdit();
    pe->setPlainText(
        QStringLiteral("QPlainTextEdit：无格式纯文本。\n"
                       "用于日志或大段内容的编辑。"));
    pe->setMinimumHeight(100);
    layout->addWidget(pe);

    outer->addWidget(
        MakeGroup(QStringLiteral("QTextEdit / QPlainTextEdit"), layout));
  }

  {
    auto* form = new QFormLayout();

    auto* spin = new QSpinBox();
    spin->setRange(0, 100);
    spin->setValue(42);
    form->addRow(QStringLiteral("QSpinBox"), spin);

    auto* dspin = new QDoubleSpinBox();
    dspin->setRange(0.0, 10.0);
    dspin->setDecimals(2);
    dspin->setValue(3.14);
    form->addRow(QStringLiteral("QDoubleSpinBox"), dspin);

    auto* date_edit = new QDateEdit(QDate::currentDate());
    date_edit->setCalendarPopup(true);
    form->addRow(QStringLiteral("QDateEdit（calendarPopup）"), date_edit);

    auto* time_edit = new QTimeEdit(QTime::currentTime());
    form->addRow(QStringLiteral("QTimeEdit"), time_edit);

    auto* dt_edit = new QDateTimeEdit(QDateTime::currentDateTime());
    dt_edit->setCalendarPopup(true);
    form->addRow(QStringLiteral("QDateTimeEdit（calendarPopup）"), dt_edit);

    outer->addWidget(MakeGroup(
        QStringLiteral("QAbstractSpinBox 族：Spin / Double / Date / Time / "
                       "DateTime"),
        form));
  }

  {
    auto* layout = new QHBoxLayout();

    auto* combo = new QComboBox();
    combo->addItems({QStringLiteral("选项一"), QStringLiteral("选项二"),
                     QStringLiteral("选项三"),
                     QStringLiteral("选项四（较长的一个选项）")});
    layout->addWidget(combo);
    layout->addStretch();

    outer->addWidget(MakeGroup(
        QStringLiteral(
            "QComboBox：弹出列表命中 QComboBox QAbstractItemView 规则"),
        layout));
  }

  outer->addStretch();
}