#include "settings_dialog.h"

#include <QLabel>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
  setObjectName(QStringLiteral("settingsDialog"));
  initUi();
  initSignals();
}

void SettingsDialog::initUi() {
  setWindowTitle(QStringLiteral("设置"));
  setFixedSize(360, 240);

  auto* layout = new QVBoxLayout(this);
  auto* title = new QLabel(QStringLiteral("设置"), this);
  title->setObjectName(QStringLiteral("pagePlaceholderTitle"));
  title->setAlignment(Qt::AlignCenter);
  auto* hint = new QLabel(
      QStringLiteral("设置项开发中，后续里程碑填充（通讯/采集/告警等）"), this);
  hint->setObjectName(QStringLiteral("pagePlaceholderHint"));
  hint->setAlignment(Qt::AlignCenter);
  layout->addStretch();
  layout->addWidget(title);
  layout->addWidget(hint);
  layout->addStretch();
}

void SettingsDialog::initSignals() {}
