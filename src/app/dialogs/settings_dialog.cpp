#include "settings_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include "config/config_defs.h"
#include "config/config_manager.h"

namespace {

// 单文件大小在界面上以 MB 呈现，配置中以字节存储
constexpr int kBytesPerMb = 1024 * 1024;

}  // namespace

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
  initUi();
  initSignals();
  loadFromConfig();
}

void SettingsDialog::initUi() {
  setObjectName(QStringLiteral("settingsDialog"));
  setWindowTitle(QStringLiteral("设置"));
  resize(420, 300);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(20, 18, 20, 16);
  layout->setSpacing(12);

  auto* form = new QFormLayout();
  form->setSpacing(10);

  log_level_box_ = new QComboBox(this);
  log_level_box_->addItems({QStringLiteral("Debug"), QStringLiteral("Info"),
                            QStringLiteral("Warn"), QStringLiteral("Error"),
                            QStringLiteral("Fatal")});
  form->addRow(QStringLiteral("日志级别"), log_level_box_);

  keep_days_box_ = new QSpinBox(this);
  keep_days_box_->setRange(1, 365);
  keep_days_box_->setSuffix(QStringLiteral(" 天"));
  form->addRow(QStringLiteral("日志保留天数"), keep_days_box_);

  max_file_size_box_ = new QSpinBox(this);
  max_file_size_box_->setRange(1, 1024);
  max_file_size_box_->setSuffix(QStringLiteral(" MB"));
  form->addRow(QStringLiteral("单文件大小上限"), max_file_size_box_);

  max_file_count_box_ = new QSpinBox(this);
  max_file_count_box_->setRange(1, 200);
  max_file_count_box_->setSuffix(QStringLiteral(" 个"));
  form->addRow(QStringLiteral("文件数上限"), max_file_count_box_);

  layout->addLayout(form);

  auto* hint = new QLabel(
      QStringLiteral("日志级别修改后立即生效；文件相关参数下次启动生效。"),
      this);
  hint->setObjectName(QStringLiteral("settingsHint"));
  hint->setWordWrap(true);
  layout->addWidget(hint);
  layout->addStretch();

  auto* buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                           this);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this,
          &SettingsDialog::onAccepted);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::initSignals() {}

void SettingsDialog::loadFromConfig() {
  auto& cfg = ConfigManager::instance();

  const int level = cfg.get<int>(QString::fromLatin1(kConfigLogLevel),
                                 kConfigLogDefaultLevel);
  log_level_box_->setCurrentIndex(qBound(0, level, 4));

  keep_days_box_->setValue(cfg.get<int>(QString::fromLatin1(kConfigLogKeepDays),
                                        kConfigLogDefaultKeepDays));
  max_file_size_box_->setValue(
      cfg.get<int>(QString::fromLatin1(kConfigLogMaxFileSize),
                   kConfigLogDefaultMaxFileSize) /
      kBytesPerMb);
  max_file_count_box_->setValue(
      cfg.get<int>(QString::fromLatin1(kConfigLogMaxFileCount),
                   kConfigLogDefaultMaxFileCount));
}

void SettingsDialog::saveToConfig() {
  auto& cfg = ConfigManager::instance();

  cfg.set<int>(QString::fromLatin1(kConfigLogLevel),
               log_level_box_->currentIndex());
  cfg.set<int>(QString::fromLatin1(kConfigLogKeepDays),
               keep_days_box_->value());
  cfg.set<int>(QString::fromLatin1(kConfigLogMaxFileSize),
               max_file_size_box_->value() * kBytesPerMb);
  cfg.set<int>(QString::fromLatin1(kConfigLogMaxFileCount),
               max_file_count_box_->value());
  cfg.sync();
}

void SettingsDialog::onAccepted() {
  saveToConfig();
  accept();
}
