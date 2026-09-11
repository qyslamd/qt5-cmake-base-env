#include "splash_screen.h"

#include <QCoreApplication>
#include <QFrame>
#include <QGuiApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QProgressBar>
#include <QScreen>
#include <QVBoxLayout>

SplashScreen::SplashScreen(QWidget* parent) : QWidget(parent) {
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setFixedSize(480, 320);
  initUi();
}

void SplashScreen::initUi() {
  // 圆角与背景由 default.qss 的 #splashContent 绘制，C++ 侧不写样式
  auto* content = new QFrame(this);
  content->setObjectName(QStringLiteral("splashContent"));

  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->addWidget(content);

  auto* layout = new QVBoxLayout(content);
  layout->setContentsMargins(32, 28, 32, 24);
  layout->setSpacing(10);

  logo_label_ = new QLabel(content);
  logo_label_->setObjectName(QStringLiteral("splashLogo"));
  logo_label_->setAlignment(Qt::AlignCenter);
  logo_label_->setFixedSize(96, 96);
  logo_label_->setPixmap(
      QIcon(QStringLiteral(":/resources/icons/icon.ico")).pixmap(96, 96));
  layout->addWidget(logo_label_, 0, Qt::AlignHCenter);

  // 标题随 applicationName 走，模板使用者改 setApplicationName 即可
  title_label_ = new QLabel(QCoreApplication::applicationName(), content);
  title_label_->setObjectName(QStringLiteral("splashTitle"));
  title_label_->setAlignment(Qt::AlignCenter);
  layout->addWidget(title_label_);

  status_label_ = new QLabel(QStringLiteral("正在启动..."), content);
  status_label_->setObjectName(QStringLiteral("splashStatus"));
  status_label_->setAlignment(Qt::AlignCenter);
  status_label_->setWordWrap(true);
  layout->addWidget(status_label_);

  progress_bar_ = new QProgressBar(content);
  progress_bar_->setObjectName(QStringLiteral("splashProgressBar"));
  progress_bar_->setRange(0, 100);
  progress_bar_->setValue(0);
  progress_bar_->setTextVisible(true);
  progress_bar_->setFormat(QStringLiteral("%p%"));
  progress_bar_->setFixedHeight(16);
  layout->addWidget(progress_bar_);
}

void SplashScreen::setStatusText(const QString& text) {
  status_label_->setText(text);
}

void SplashScreen::setProgress(int percent) {
  progress_bar_->setValue(percent);
}

void SplashScreen::finish() { hide(); }

void SplashScreen::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  // 无边框窗口需自行居中于主屏
  if (QScreen* screen = QGuiApplication::primaryScreen()) {
    const QRect available = screen->availableGeometry();
    move(available.center() - rect().center());
  }
}

void SplashScreen::mousePressEvent(QMouseEvent* event) { event->accept(); }

void SplashScreen::keyPressEvent(QKeyEvent* event) { event->accept(); }
