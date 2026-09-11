#include "about_dialog.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

#include "AppVersion.h"

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
  initUi();
  initSignals();
}

void AboutDialog::initUi() {
  setObjectName(QStringLiteral("aboutDialog"));
  setWindowTitle(QStringLiteral("关于"));
  setFixedSize(360, 280);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 20, 24, 16);
  layout->setSpacing(8);

  auto* app_name = new QLabel(QCoreApplication::applicationName(), this);
  app_name->setObjectName(QStringLiteral("aboutAppName"));
  app_name->setAlignment(Qt::AlignCenter);

  // 版本与构建信息来自 cmake/version.h.in 生成的 AppVersion.h
  auto* info = new QLabel(
      QStringLiteral("版本：%1\n构建时间：%2\nQt 版本：%3")
          .arg(QString::fromLatin1(getProjectVersionAll()),
               QString::fromLatin1(getProjectBuildInfo()),
               QString::fromLatin1(qVersion())),
      this);
  info->setObjectName(QStringLiteral("aboutInfo"));
  info->setAlignment(Qt::AlignCenter);

  auto* hint = new QLabel(
      QStringLiteral("Qt5 + CMake + Ninja 项目基础模板"), this);
  hint->setObjectName(QStringLiteral("aboutHint"));
  hint->setAlignment(Qt::AlignCenter);

  layout->addWidget(app_name);
  layout->addWidget(info);
  layout->addWidget(hint);
  layout->addStretch();

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

void AboutDialog::initSignals() {}
