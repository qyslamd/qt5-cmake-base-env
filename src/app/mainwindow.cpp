#include "mainwindow.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  initUi();
  initSignals();
}

MainWindow::~MainWindow() {}

void MainWindow::initUi() {
  setWindowTitle(QStringLiteral("EP-H Demo Qt application framework"));
  setMinimumSize(800, 600);
}

void MainWindow::initSignals() {}
