#include "log_filter_bar.h"

#include <spdlog/spdlog.h>

#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>

LogFilterBar::LogFilterBar(QWidget* parent) : QWidget(parent) {
  // 级别定义（单字母按钮 + spdlog level + objectName 后缀）
  level_defs_ = {
      {QStringLiteral("D"), spdlog::level::debug, QStringLiteral("Debug"),
       QStringLiteral("DEBUG")},
      {QStringLiteral("I"), spdlog::level::info, QStringLiteral("Info"),
       QStringLiteral("INFO")},
      {QStringLiteral("W"), spdlog::level::warn, QStringLiteral("Warn"),
       QStringLiteral("WARN")},
      {QStringLiteral("E"), spdlog::level::err, QStringLiteral("Error"),
       QStringLiteral("ERROR")},
      {QStringLiteral("F"), spdlog::level::critical, QStringLiteral("Fatal"),
       QStringLiteral("FATAL")},
  };
  initUi();
  initSignals();
}

void LogFilterBar::initUi() {
  setObjectName(QStringLiteral("LogFilterBar"));

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(4, 2, 4, 2);
  layout->setSpacing(4);

  // 级别按钮（默认全选）
  for (const auto& def : level_defs_) {
    auto* btn = new QToolButton(this);
    btn->setObjectName(QStringLiteral("Level%1Btn").arg(def.object_suffix));
    btn->setText(def.name);
    btn->setCheckable(true);
    btn->setChecked(true);
    btn->setAutoRaise(true);
    btn->setToolTip(QStringLiteral("过滤 %1 级别").arg(def.full_name));
    level_buttons_[def.level] = btn;
    layout->addWidget(btn);
  }
  layout->addSpacing(8);

  // 过滤输入框
  filter_edit_ = new QLineEdit(this);
  filter_edit_->setObjectName(QStringLiteral("LogFilterBox"));
  filter_edit_->setClearButtonEnabled(true);
  filter_edit_->setPlaceholderText(QStringLiteral("过滤..."));
  layout->addWidget(filter_edit_, 1);

  // 正则开关
  regex_btn_ = new QToolButton(this);
  regex_btn_->setObjectName(QStringLiteral("RegexBtn"));
  regex_btn_->setText(QStringLiteral(".*"));
  regex_btn_->setCheckable(true);
  regex_btn_->setAutoRaise(true);
  regex_btn_->setToolTip(QStringLiteral("正则表达式"));
  layout->addWidget(regex_btn_);

  // 大小写开关
  case_btn_ = new QToolButton(this);
  case_btn_->setObjectName(QStringLiteral("CaseBtn"));
  case_btn_->setText(QStringLiteral("Aa"));
  case_btn_->setCheckable(true);
  case_btn_->setAutoRaise(true);
  case_btn_->setToolTip(QStringLiteral("区分大小写"));
  layout->addWidget(case_btn_);

  // 滚动锁定
  lock_btn_ = new QToolButton(this);
  lock_btn_->setObjectName(QStringLiteral("LockBtn"));
  lock_btn_->setText(QStringLiteral("锁"));
  lock_btn_->setCheckable(true);
  lock_btn_->setAutoRaise(true);
  lock_btn_->setToolTip(QStringLiteral("滚动锁定"));
  layout->addWidget(lock_btn_);

  // 清空
  clear_btn_ = new QToolButton(this);
  clear_btn_->setObjectName(QStringLiteral("ClearBtn"));
  clear_btn_->setText(QStringLiteral("清"));
  clear_btn_->setAutoRaise(true);
  clear_btn_->setToolTip(QStringLiteral("清空"));
  layout->addWidget(clear_btn_);
}

void LogFilterBar::initSignals() {
  // 级别按钮 toggled
  for (const auto& def : level_defs_) {
    auto* btn = level_buttons_[def.level];
    connect(btn, &QToolButton::toggled, this,
            [this, level = def.level](bool checked) {
              onLevelToggled(level, checked);
            });
  }

  // 过滤框文本变化（300ms 防抖）
  auto* debounce = new QTimer(this);
  debounce->setSingleShot(true);
  debounce->setInterval(300);
  connect(filter_edit_, &QLineEdit::textChanged, debounce,
          QOverload<>::of(&QTimer::start));
  connect(debounce, &QTimer::timeout, this,
          [this]() { emit filterChanged(filter()); });

  // 正则 / 大小写开关
  connect(regex_btn_, &QToolButton::toggled, this,
          [this]() { emit filterChanged(filter()); });
  connect(case_btn_, &QToolButton::toggled, this,
          [this]() { emit filterChanged(filter()); });

  // 滚动锁定
  connect(lock_btn_, &QToolButton::toggled, this, [this](bool checked) {
    scroll_locked_ = checked;
    emit scrollLockChanged(checked);
  });

  // 清空
  connect(clear_btn_, &QToolButton::clicked, this,
          [this]() { emit clearRequested(); });
}

LogFilter LogFilterBar::filter() const {
  LogFilter f;
  f.text = filter_edit_->text();
  f.use_regex = regex_btn_->isChecked();
  f.case_sensitive = case_btn_->isChecked();
  for (auto it = level_buttons_.constBegin(); it != level_buttons_.constEnd();
       ++it) {
    if (it.value()->isChecked()) {
      f.enabled_levels.insert(it.key());
    }
  }
  return f;
}

void LogFilterBar::setFilterInvalid(bool invalid, const QString& error) {
  if (invalid == last_invalid_ && error == last_error_) {
    return;
  }
  last_invalid_ = invalid;
  last_error_ = error;
  filter_edit_->setProperty("invalid", invalid);
  filter_edit_->setToolTip(error);
  filter_edit_->style()->unpolish(filter_edit_);
  filter_edit_->style()->polish(filter_edit_);
}

void LogFilterBar::onLevelToggled(int level, bool checked) {
  if (!checked) {
    int checked_count = 0;
    for (auto it = level_buttons_.constBegin(); it != level_buttons_.constEnd();
         ++it) {
      if (it.value()->isChecked()) {
        ++checked_count;
      }
    }
    if (checked_count == 0) {
      QToolButton* btn = level_buttons_.value(level);
      QSignalBlocker blocker(btn);
      btn->setChecked(true);
      return;
    }
  }
  emit filterChanged(filter());
}
