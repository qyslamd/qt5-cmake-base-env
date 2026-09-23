// 样式表样例：把底座层（src/style/resources/styles/base/*.qss）覆盖到的控件
// 按族摆出来，改样式之后开这个程序点一遍就能看出哪一类被改坏了。
//
// 与 demo_app 的分工：demo_app 的业务控件（登录、设置、日志面板…）验的是**项目层**
// （src/app/resources/styles/default.qss），那些 objectName 只有它自己有；
// 这里只验**底座层**，任何项目拿去都长这样。
//
// 这里不写一行 setStyleSheet——样式全在 QSS 里，代码只负责把控件摆出来，
// 需要区分的角色用动态属性（role="primary"/"secondary"/"danger"）。
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimeEdit>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <style.h>

namespace {

const int kCaptionWidth = 120;

// 页面开头两行说明：这一页验哪份表、要点是什么
QWidget* MakeHeader(const QString& title, const QString& covers,
                    const QString& note) {
  auto* header = new QWidget;
  auto* layout = new QVBoxLayout(header);
  layout->setContentsMargins(0, 0, 0, 8);

  auto* title_label = new QLabel(title);
  title_label->setObjectName(QStringLiteral("demoPageTitle"));
  layout->addWidget(title_label);

  auto* covers_label = new QLabel(QStringLiteral("覆盖：%1").arg(covers));
  covers_label->setProperty("role", "hint");
  layout->addWidget(covers_label);

  if (!note.isEmpty()) {
    auto* note_label = new QLabel(note);
    note_label->setProperty("role", "hint");
    note_label->setWordWrap(true);
    layout->addWidget(note_label);
  }
  return header;
}

// 一行：左边固定宽度的说明，右边是要看的控件
void AddRow(QVBoxLayout* layout, const QString& caption, QWidget* widget) {
  auto* row = new QHBoxLayout;
  auto* caption_label = new QLabel(caption);
  caption_label->setMinimumWidth(kCaptionWidth);
  caption_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  row->addWidget(caption_label);
  row->addWidget(widget, 1);
  layout->addLayout(row);
}

QWidget* MakeCard(const QString& caption, QWidget* content) {
  auto* box = new QGroupBox(caption);
  auto* layout = new QVBoxLayout(box);
  layout->addWidget(content);
  return box;
}

QWidget* MakeButtonPage() {
  auto* page = new QWidget;
  auto* layout = new QVBoxLayout(page);
  layout->addWidget(MakeHeader(
      QStringLiteral("按钮"), QStringLiteral("styles/base/button.qss"),
      QStringLiteral("QPushButton 默认就是主按钮；要次按钮、危险按钮用 "
                     "role 属性。QToolButton 默认中性，工具条与面板用它。")));

  auto* push_default = new QPushButton(QStringLiteral("默认（主按钮）"));
  auto* push_secondary = new QPushButton(QStringLiteral("次按钮"));
  push_secondary->setProperty("role", "secondary");
  auto* push_danger = new QPushButton(QStringLiteral("危险按钮"));
  push_danger->setProperty("role", "danger");
  auto* push_disabled = new QPushButton(QStringLiteral("禁用"));
  push_disabled->setEnabled(false);

  AddRow(layout, QStringLiteral("QPushButton"), push_default);
  AddRow(layout, QStringLiteral("role=secondary"), push_secondary);
  AddRow(layout, QStringLiteral("role=danger"), push_danger);
  AddRow(layout, QStringLiteral("禁用"), push_disabled);

  auto* tool_default = new QToolButton;
  tool_default->setText(QStringLiteral("工具按钮"));
  auto* tool_checked = new QToolButton;
  tool_checked->setText(QStringLiteral("选中态"));
  tool_checked->setCheckable(true);
  tool_checked->setChecked(true);
  auto* tool_primary = new QToolButton;
  tool_primary->setText(QStringLiteral("主按钮（QToolButton）"));
  tool_primary->setProperty("role", "primary");
  auto* tool_disabled = new QToolButton;
  tool_disabled->setText(QStringLiteral("禁用"));
  tool_disabled->setEnabled(false);

  AddRow(layout, QStringLiteral("QToolButton"), tool_default);
  AddRow(layout, QStringLiteral(":checked"), tool_checked);
  AddRow(layout, QStringLiteral("role=primary"), tool_primary);
  AddRow(layout, QStringLiteral("禁用"), tool_disabled);

  auto* check_off = new QCheckBox(QStringLiteral("未勾选"));
  auto* check_on = new QCheckBox(QStringLiteral("已勾选"));
  check_on->setChecked(true);
  auto* check_disabled = new QCheckBox(QStringLiteral("禁用"));
  check_disabled->setEnabled(false);
  AddRow(layout, QStringLiteral("QCheckBox"), check_off);
  AddRow(layout, QStringLiteral(":checked"), check_on);
  AddRow(layout, QStringLiteral("禁用"), check_disabled);

  auto* radio_off = new QRadioButton(QStringLiteral("未选中"));
  auto* radio_on = new QRadioButton(QStringLiteral("已选中"));
  radio_on->setChecked(true);
  auto* radio_disabled = new QRadioButton(QStringLiteral("禁用"));
  radio_disabled->setEnabled(false);
  AddRow(layout, QStringLiteral("QRadioButton"), radio_off);
  AddRow(layout, QStringLiteral(":checked"), radio_on);
  AddRow(layout, QStringLiteral("禁用"), radio_disabled);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                       QDialogButtonBox::Cancel);
  AddRow(layout, QStringLiteral("QDialogButtonBox"), buttons);

  layout->addStretch();
  return page;
}

QWidget* MakeInputPage() {
  auto* page = new QWidget;
  auto* layout = new QVBoxLayout(page);
  layout->addWidget(MakeHeader(
      QStringLiteral("输入"), QStringLiteral("styles/base/input.qss"),
      QStringLiteral("下拉箭头与微调箭头沿用 Qt 原生绘制（自绘需要额外备两套"
                     "箭头 SVG）。错误态用动态属性 error。")));

  auto* edit_plain = new QLineEdit(QStringLiteral("普通输入"));
  auto* edit_placeholder = new QLineEdit;
  edit_placeholder->setPlaceholderText(QStringLiteral("占位文本"));
  auto* edit_form = new QLineEdit(QStringLiteral("表单输入（role=form）"));
  edit_form->setProperty("role", "form");
  auto* edit_error = new QLineEdit(QStringLiteral("错误态"));
  edit_error->setProperty("error", "true");
  auto* edit_disabled = new QLineEdit(QStringLiteral("禁用"));
  edit_disabled->setEnabled(false);

  AddRow(layout, QStringLiteral("QLineEdit"), edit_plain);
  AddRow(layout, QStringLiteral("占位文本"), edit_placeholder);
  AddRow(layout, QStringLiteral("role=form"), edit_form);
  AddRow(layout, QStringLiteral("error=true"), edit_error);
  AddRow(layout, QStringLiteral("禁用"), edit_disabled);

  auto* text_edit = new QTextEdit(QStringLiteral("多行文本，带边框与圆角"));
  text_edit->setMaximumHeight(70);
  AddRow(layout, QStringLiteral("QTextEdit"), text_edit);

  auto* plain_edit = new QPlainTextEdit(QStringLiteral("纯文本编辑"));
  plain_edit->setMaximumHeight(70);
  AddRow(layout, QStringLiteral("QPlainTextEdit"), plain_edit);

  auto* combo = new QComboBox;
  combo->addItems({QStringLiteral("第一项"), QStringLiteral("第二项"),
                   QStringLiteral("第三项")});
  auto* combo_disabled = new QComboBox;
  combo_disabled->addItem(QStringLiteral("禁用"));
  combo_disabled->setEnabled(false);
  AddRow(layout, QStringLiteral("QComboBox"), combo);
  AddRow(layout, QStringLiteral("禁用"), combo_disabled);

  AddRow(layout, QStringLiteral("QSpinBox"), new QSpinBox);
  AddRow(layout, QStringLiteral("QDoubleSpinBox"), new QDoubleSpinBox);
  AddRow(layout, QStringLiteral("QDateEdit"), new QDateEdit);
  AddRow(layout, QStringLiteral("QTimeEdit"), new QTimeEdit);
  AddRow(layout, QStringLiteral("QDateTimeEdit"), new QDateTimeEdit);

  layout->addStretch();
  return page;
}

QWidget* MakeItemViewPage() {
  auto* page = new QWidget;
  auto* layout = new QVBoxLayout(page);
  layout->addWidget(MakeHeader(
      QStringLiteral("列表、表格、树"), QStringLiteral("styles/base/itemview.qss"),
      QStringLiteral("表头由 QHeaderView::section 统一给；下拉框的弹出列表由 "
                     "input.qss 里具体度更高的规则接管。")));

  auto* list = new QListWidget;
  for (int i = 1; i <= 4; ++i) {
    list->addItem(QStringLiteral("列表项 %1").arg(i));
  }
  list->setCurrentRow(1);
  list->setMaximumHeight(120);
  AddRow(layout, QStringLiteral("QListWidget"), list);

  auto* tree = new QTreeWidget;
  tree->setColumnCount(2);
  tree->setHeaderLabels({QStringLiteral("名称"), QStringLiteral("值")});
  auto* top = new QTreeWidgetItem(tree, {QStringLiteral("父节点"),
                                         QStringLiteral("1")});
  new QTreeWidgetItem(top, {QStringLiteral("子节点"),
                            QStringLiteral("2")});
  tree->expandAll();
  tree->setMaximumHeight(120);
  AddRow(layout, QStringLiteral("QTreeWidget"), tree);

  auto* table = new QTableWidget(3, 3);
  table->setHorizontalHeaderLabels({QStringLiteral("列一"),
                                    QStringLiteral("列二"),
                                    QStringLiteral("列三")});
  table->setAlternatingRowColors(true);
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      table->setItem(row, col,
                     new QTableWidgetItem(QStringLiteral("单元格 %1-%2")
                                              .arg(row + 1)
                                              .arg(col + 1)));
    }
  }
  table->selectRow(1);
  table->setMaximumHeight(150);
  AddRow(layout, QStringLiteral("QTableWidget"), table);

  layout->addStretch();
  return page;
}

QWidget* MakeContainerPage() {
  auto* page = new QWidget;
  auto* layout = new QVBoxLayout(page);
  layout->addWidget(MakeHeader(
      QStringLiteral("容器、选项卡、进度与滑块"),
      QStringLiteral("styles/base/container.qss、tab.qss、progress.qss"),
      QStringLiteral("QFrame 故意不给规则：SARibbon 的面板与画廊都是它的子类，"
                     "一条裸规则会直接改功能区。")));

  auto* card_content = new QWidget;
  auto* card_layout = new QVBoxLayout(card_content);
  card_layout->addWidget(new QLabel(QStringLiteral("分组框里的内容")));
  card_layout->addWidget(new QPushButton(QStringLiteral("按一下")));
  AddRow(layout, QStringLiteral("QGroupBox"), MakeCard(QStringLiteral("分组"),
                                                       card_content));

  auto* splitter = new QSplitter(Qt::Horizontal);
  auto* splitter_left = new QLabel(QStringLiteral("左"));
  splitter_left->setAlignment(Qt::AlignCenter);
  auto* splitter_right = new QLabel(QStringLiteral("右"));
  splitter_right->setAlignment(Qt::AlignCenter);
  splitter->addWidget(splitter_left);
  splitter->addWidget(splitter_right);
  splitter->setMinimumHeight(70);
  AddRow(layout, QStringLiteral("QSplitter"), splitter);

  auto* scroll_area = new QScrollArea;
  scroll_area->setWidget(new QLabel(QStringLiteral("滚动区里的内容")));
  scroll_area->setMinimumHeight(70);
  AddRow(layout, QStringLiteral("QScrollArea"), scroll_area);

  auto* tabs = new QTabWidget;
  tabs->addTab(new QLabel(QStringLiteral("第一页")), QStringLiteral("标签一"));
  tabs->addTab(new QLabel(QStringLiteral("第二页")), QStringLiteral("标签二"));
  tabs->setMinimumHeight(110);
  AddRow(layout, QStringLiteral("QTabWidget"), tabs);

  auto* progress_0 = new QProgressBar;
  progress_0->setValue(0);
  auto* progress_50 = new QProgressBar;
  progress_50->setValue(50);
  auto* progress_100 = new QProgressBar;
  progress_100->setValue(100);
  auto* progress_disabled = new QProgressBar;
  progress_disabled->setValue(30);
  progress_disabled->setEnabled(false);
  AddRow(layout, QStringLiteral("QProgressBar 0%"), progress_0);
  AddRow(layout, QStringLiteral("50%"), progress_50);
  AddRow(layout, QStringLiteral("100%"), progress_100);
  AddRow(layout, QStringLiteral("禁用"), progress_disabled);

  auto* slider = new QSlider(Qt::Horizontal);
  slider->setValue(40);
  AddRow(layout, QStringLiteral("QSlider"), slider);

  layout->addStretch();
  return page;
}

QWidget* MakeBarPage() {
  auto* page = new QWidget;
  auto* layout = new QVBoxLayout(page);
  layout->addWidget(MakeHeader(
      QStringLiteral("菜单与栏"), QStringLiteral("styles/base/menu.qss"),
      QStringLiteral("菜单栏在窗口顶部、工具条与状态栏也已在本窗口里；这页只补"
                     "弹出菜单与工具提示，工具提示要悬停才出现。")));

  auto* menu_button = new QPushButton(QStringLiteral("点开菜单"));
  auto* menu = new QMenu(menu_button);
  menu->addAction(QStringLiteral("普通项"));
  menu->addAction(QStringLiteral("带图标的项"))->setEnabled(true);
  menu->addSeparator();
  QAction* disabled_action = menu->addAction(QStringLiteral("禁用项"));
  disabled_action->setEnabled(false);
  menu_button->setMenu(menu);
  AddRow(layout, QStringLiteral("QMenu"), menu_button);

  auto* tip = new QLabel(QStringLiteral("把鼠标停在这里看 QToolTip"));
  tip->setToolTip(QStringLiteral("这是 QToolTip 的样子"));
  AddRow(layout, QStringLiteral("QToolTip"), tip);

  auto* bar_note = new QLabel(QStringLiteral(
      "本窗口顶部的 QMenuBar 与 QToolBar、底部的 QStatusBar 都属于这一层。"));
  bar_note->setProperty("role", "hint");
  layout->addWidget(bar_note);

  layout->addStretch();
  return page;
}

QWidget* MakeLabelPage() {
  auto* page = new QWidget;
  auto* layout = new QVBoxLayout(page);
  layout->addWidget(MakeHeader(
      QStringLiteral("文本与视图"), QStringLiteral("styles/base/label.qss、widget.qss"),
      QStringLiteral("QLabel 的文本色最容易在改底座层时到处变；图形视图的边框"
                     "由 widget.qss 给。")));

  auto* plain = new QLabel(QStringLiteral("普通文本"));
  auto* hint = new QLabel(QStringLiteral("提示文本（role=hint）"));
  hint->setProperty("role", "hint");
  auto* error = new QLabel(QStringLiteral("报错文本（role=error）"));
  error->setProperty("role", "error");
  auto* disabled = new QLabel(QStringLiteral("禁用"));
  disabled->setEnabled(false);

  AddRow(layout, QStringLiteral("QLabel"), plain);
  AddRow(layout, QStringLiteral("role=hint"), hint);
  AddRow(layout, QStringLiteral("role=error"), error);
  AddRow(layout, QStringLiteral("禁用"), disabled);

  auto* view = new QGraphicsView;
  auto* scene = new QGraphicsScene(view);
  scene->addText(QStringLiteral("QGraphicsView"));
  view->setScene(scene);
  view->setMinimumHeight(120);
  AddRow(layout, QStringLiteral("QGraphicsView"), view);

  auto* note = new QLabel(QStringLiteral(
      "QMainWindow::separator（停靠窗口之间的分隔条）要主窗口有 QDockWidget 才"
      "看得到，本页不演示。"));
  note->setProperty("role", "hint");
  note->setWordWrap(true);
  layout->addWidget(note);

  layout->addStretch();
  return page;
}

// 页面统一套一层滚动区：控件摆得比窗口高时也能看全
QWidget* WrapInScroll(QWidget* page) {
  auto* area = new QScrollArea;
  area->setWidgetResizable(true);
  area->setWidget(page);
  return area;
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  // 底座层（本程序不带项目层：那些 objectName 规则属于 demo_app 的业务控件）
  ApplyStyleSheet(app);

  QMainWindow window;
  window.setWindowTitle(
      QStringLiteral("样式表样例 - 底座层（src/style/resources/styles/base）"));
  window.resize(900, 760);

  // 菜单栏与工具条：验 menu.qss 的 QMenuBar / QToolBar / QStatusBar
  QMenu* file_menu = window.menuBar()->addMenu(QStringLiteral("文件"));
  file_menu->addAction(QStringLiteral("新建"));
  file_menu->addAction(QStringLiteral("打开"))->setEnabled(false);
  file_menu->addSeparator();
  file_menu->addAction(QStringLiteral("退出"), &window, &QWidget::close);

  QMenu* help_menu = window.menuBar()->addMenu(QStringLiteral("帮助"));
  help_menu->addAction(QStringLiteral("关于"));

  QToolBar* tool_bar = window.addToolBar(QStringLiteral("主工具条"));
  tool_bar->addAction(QStringLiteral("动作一"));
  tool_bar->addAction(QStringLiteral("动作二"));
  tool_bar->addSeparator();
  QAction* disabled_action = tool_bar->addAction(QStringLiteral("禁用动作"));
  disabled_action->setEnabled(false);

  window.statusBar()->showMessage(
      QStringLiteral("状态栏：样式来自 src/style/resources/styles/base/*.qss"));

  auto* tabs = new QTabWidget;
  tabs->addTab(WrapInScroll(MakeButtonPage()), QStringLiteral("按钮"));
  tabs->addTab(WrapInScroll(MakeInputPage()), QStringLiteral("输入"));
  tabs->addTab(WrapInScroll(MakeItemViewPage()), QStringLiteral("列表"));
  tabs->addTab(WrapInScroll(MakeContainerPage()), QStringLiteral("容器"));
  tabs->addTab(WrapInScroll(MakeBarPage()), QStringLiteral("菜单与栏"));
  tabs->addTab(WrapInScroll(MakeLabelPage()), QStringLiteral("文本"));
  window.setCentralWidget(tabs);

  window.show();
  return app.exec();
}
