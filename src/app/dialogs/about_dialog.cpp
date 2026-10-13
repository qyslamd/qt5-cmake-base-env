#include "about_dialog.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

#include "AppVersion.h"
#include "launch_options.h"

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
  initUi();
  initSignals();
}

void AboutDialog::initUi() {
  setObjectName(QStringLiteral("aboutDialog"));
  setWindowTitle(QStringLiteral("关于"));
  // 不固定尺寸：命令行有多长随实参而变，可调才不会被挤爆
  setMinimumSize(420, 300);
  resize(480, 360);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 20, 24, 16);
  layout->setSpacing(8);

  auto* app_name = new QLabel(QCoreApplication::applicationName(), this);
  app_name->setObjectName(QStringLiteral("aboutAppName"));
  app_name->setAlignment(Qt::AlignCenter);

  // 版本与构建信息来自 cmake/version.h.in 生成的 AppVersion.h
  auto* info = new QLabel(QStringLiteral("版本：%1\n构建时间：%2\nQt 版本：%3")
                              .arg(QString::fromLatin1(getProjectVersionAll()),
                                   QString::fromLatin1(getProjectBuildInfo()),
                                   QString::fromLatin1(qVersion())),
                          this);
  info->setObjectName(QStringLiteral("aboutInfo"));
  info->setAlignment(Qt::AlignCenter);

  auto* hint =
      new QLabel(QStringLiteral("Qt5 + CMake + Ninja 项目基础模板"), this);
  hint->setObjectName(QStringLiteral("aboutHint"));
  hint->setAlignment(Qt::AlignCenter);

  layout->addWidget(app_name);
  layout->addWidget(info);
  layout->addWidget(hint);
  layout->addSpacing(12);

  // 本次运行的实际命令行（不是可用参数清单——那是 -h/--help 的事）：
  // 排查"这个进程是怎么被拉起来的"时一眼可见，也可选中复制去复现
  auto* cli_title = new QLabel(QStringLiteral("本次运行的命令行"), this);
  cli_title->setObjectName(QStringLiteral("aboutCommandLineTitle"));
  layout->addWidget(cli_title);

  auto* cli_text = new QLabel(CommandLineText(), this);
  cli_text->setObjectName(QStringLiteral("aboutCommandLine"));
  // 可选中是为了直接复制去终端复现；路径里可能有 Windows 反斜杠与空格，
  // 按 PlainText 原样显示，别被 QLabel 当富文本解析
  cli_text->setTextFormat(Qt::PlainText);
  cli_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
  cli_text->setWordWrap(true);
  cli_text->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  // 含路径与参数，等宽更好读；取系统固定的等宽字体，不写死字体名
  cli_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  layout->addWidget(cli_text, 1);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

void AboutDialog::initSignals() {}
