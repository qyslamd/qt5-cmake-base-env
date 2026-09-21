#include "settings_dialog.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>

#include <qt-libui/buttons/switch_button.h>

#include "config/config_defs.h"
#include "config/config_manager.h"

namespace {

// 单文件大小在界面上以 MB 呈现，配置中以字节存储
constexpr int kBytesPerMb = 1024 * 1024;

// 日志级别下拉项，索引即 kConfigLogLevel 的取值
const QStringList& LogLevelItems() {
  static const QStringList kItems = {
      QStringLiteral("Debug"), QStringLiteral("Info"), QStringLiteral("Warn"),
      QStringLiteral("Error"), QStringLiteral("Fatal")};
  return kItems;
}

}  // namespace

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
  initUi();
  initSignals();
}

void SettingsDialog::initUi() {
  setObjectName(QStringLiteral("settingsDialog"));
  setWindowTitle(QStringLiteral("设置"));
  resize(760, 520);

  auto* root = new QHBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  // 左侧分类导航
  category_list_ = new QListWidget(this);
  category_list_->setObjectName(QStringLiteral("settingsCategoryList"));
  category_list_->setFixedWidth(150);

  // 右侧分页区
  page_stack_ = new QStackedWidget(this);
  page_stack_->setObjectName(QStringLiteral("settingsPageStack"));

  auto* right = new QVBoxLayout();
  right->setContentsMargins(0, 0, 0, 0);
  right->setSpacing(0);
  right->addWidget(page_stack_, 1);

  auto* button_row = new QHBoxLayout();
  button_row->setContentsMargins(16, 8, 16, 12);
  button_row->addStretch();
  auto* close_button = new QPushButton(QStringLiteral("关闭"), this);
  close_button->setObjectName(QStringLiteral("settingsCloseButton"));
  connect(close_button, &QPushButton::clicked, this, &QDialog::accept);
  button_row->addWidget(close_button);
  right->addLayout(button_row);

  root->addWidget(category_list_);
  root->addLayout(right, 1);

  addCategoryPage(QStringLiteral("日志"), createLogPage());
  addCategoryPage(QStringLiteral("日志格式"), createLogFormatPage());

  category_list_->setCurrentRow(0);
}

void SettingsDialog::initSignals() {
  connect(category_list_, &QListWidget::currentRowChanged, page_stack_,
          &QStackedWidget::setCurrentIndex);
}

void SettingsDialog::addCategoryPage(const QString& name, QWidget* page) {
  page_stack_->addWidget(page);
  category_list_->addItem(name);
}

QWidget* SettingsDialog::createLogPage() {
  auto* page = new QWidget(this);
  // 页面必须设 objectName。funq 的控件路径靠它定位，没有就退回「类名 +
  // 同名兄弟的序号」，而序号按 children() 顺序数——这些页面是先 new 出来、
  // 再被 addWidget reparent 进去的，setParent 会把子对象插到列表前面，
  // 序号与添加顺序正好相反（实测「日志」页成了 QWidget-1，路径没法推）
  page->setObjectName(QStringLiteral("settingsPageLog"));
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);

  addPageHeader(layout, QStringLiteral("日志"),
                QStringLiteral("日志级别与文件轮转策略"));

  auto* card = createSettingsCard(page, QStringLiteral("基本"));
  addComboBoxRow(card, QStringLiteral("日志级别"),
                 QStringLiteral("低于该级别的日志不输出，修改后立即生效"),
                 QString::fromLatin1(kConfigLogLevel), LogLevelItems(),
                 kConfigLogDefaultLevel);

  auto* keep_days =
      addSpinBoxRow(card, QStringLiteral("保留天数"),
                    QStringLiteral("启动时清理超期的历史日志文件"),
                    QString::fromLatin1(kConfigLogKeepDays), 1, 365,
                    kConfigLogDefaultKeepDays);
  keep_days->setSuffix(QStringLiteral(" 天"));

  auto* max_size = addSpinBoxRow(
      card, QStringLiteral("单文件大小上限"),
      QStringLiteral("单个日志文件超过该大小即滚动，下次启动生效"),
      QString::fromLatin1(kConfigLogMaxFileSize), 1, 1024,
      kConfigLogDefaultMaxFileSize / kBytesPerMb, kBytesPerMb);
  max_size->setSuffix(QStringLiteral(" MB"));

  auto* max_count =
      addSpinBoxRow(card, QStringLiteral("文件数上限"),
                    QStringLiteral("滚动保留的日志文件数量，下次启动生效"),
                    QString::fromLatin1(kConfigLogMaxFileCount), 1, 200,
                    kConfigLogDefaultMaxFileCount);
  max_count->setSuffix(QStringLiteral(" 个"));

  layout->addWidget(card);
  layout->addStretch();
  return page;
}

QWidget* SettingsDialog::createLogFormatPage() {
  auto* page = new QWidget(this);
  // 同上：objectName 是 funq 定位控件的依据
  page->setObjectName(QStringLiteral("settingsPageLogFormat"));
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);

  addPageHeader(layout, QStringLiteral("日志格式"),
                QStringLiteral("按需增减每行日志携带的信息，修改后立即生效"));

  auto* card = createSettingsCard(page, QStringLiteral("字段开关"));
  addCheckBoxRow(
      card, QStringLiteral("时间"), QStringLiteral("[%Y-%m-%d %H:%M:%S.%e]"),
      QString::fromLatin1(kConfigLogFmtTime), kConfigLogFmtDefaultTime);
  addCheckBoxRow(
      card, QStringLiteral("级别"), QStringLiteral("[%l]，控制台以颜色区分"),
      QString::fromLatin1(kConfigLogFmtLevel), kConfigLogFmtDefaultLevel);
  addCheckBoxRow(
      card, QStringLiteral("线程 ID"), QStringLiteral("[%t]，多线程排查时开启"),
      QString::fromLatin1(kConfigLogFmtThread), kConfigLogFmtDefaultThread);
  addCheckBoxRow(card, QStringLiteral("模块"),
                 QStringLiteral("[%n]，LOG_INFO 宏的第一个参数"),
                 QString::fromLatin1(kConfigLogFmtModule),
                 kConfigLogFmtDefaultModule);
  addCheckBoxRow(card, QStringLiteral("源文件与行号"),
                 QStringLiteral("[%s:%#]，定位调用点"),
                 QString::fromLatin1(kConfigLogFmtSource),
                 kConfigLogFmtDefaultSource);
  addCheckBoxRow(card, QStringLiteral("函数名"), QStringLiteral("[%!]"),
                 QString::fromLatin1(kConfigLogFmtFunc),
                 kConfigLogFmtDefaultFunc);
  addCheckBoxRow(
      card, QStringLiteral("消息"), QStringLiteral("%v，关闭后仅剩格式字段"),
      QString::fromLatin1(kConfigLogFmtMessage), kConfigLogFmtDefaultMessage);

  layout->addWidget(card);
  layout->addStretch();
  return page;
}

void SettingsDialog::addPageHeader(QVBoxLayout* layout, const QString& title,
                                   const QString& subtitle) {
  auto* title_label = new QLabel(title, layout->parentWidget());
  title_label->setObjectName(QStringLiteral("settingsPageTitle"));
  auto* sub_label = new QLabel(subtitle, layout->parentWidget());
  sub_label->setObjectName(QStringLiteral("settingsPageSub"));
  layout->addWidget(title_label);
  layout->addWidget(sub_label);
  layout->addSpacing(6);
}

QWidget* SettingsDialog::createSettingsCard(QWidget* parent,
                                            const QString& title) {
  // 必须是 QFrame 而非 QWidget：QSS 的背景与边框对普通 QWidget 不生效
  auto* card = new QFrame(parent);
  card->setObjectName(QStringLiteral("settingsCard"));

  auto* card_layout = new QVBoxLayout(card);
  card_layout->setContentsMargins(16, 12, 16, 12);
  card_layout->setSpacing(0);

  auto* title_label = new QLabel(title, card);
  title_label->setObjectName(QStringLiteral("settingsCardTitle"));
  card_layout->addWidget(title_label);
  return card;
}

QHBoxLayout* SettingsDialog::addSettingRow(QWidget* parent,
                                           const QString& title,
                                           const QString& description) {
  auto* row = new QWidget(parent);
  row->setObjectName(QStringLiteral("settingsRow"));

  auto* row_layout = new QHBoxLayout(row);
  row_layout->setContentsMargins(0, 8, 0, 8);
  row_layout->setSpacing(12);

  auto* left = new QVBoxLayout();
  left->setContentsMargins(0, 0, 0, 0);
  left->setSpacing(2);

  auto* title_label = new QLabel(title, row);
  title_label->setObjectName(QStringLiteral("settingsRowTitle"));
  left->addWidget(title_label);

  if (!description.isEmpty()) {
    auto* desc_label = new QLabel(description, row);
    desc_label->setObjectName(QStringLiteral("settingsRowDesc"));
    desc_label->setWordWrap(true);
    left->addWidget(desc_label);
  }

  row_layout->addLayout(left, 1);

  auto* right = new QHBoxLayout();
  right->setContentsMargins(0, 0, 0, 0);
  row_layout->addLayout(right, 0);

  parent->layout()->addWidget(row);
  return right;
}

QSpinBox* SettingsDialog::addSpinBoxRow(QWidget* parent, const QString& title,
                                        const QString& description,
                                        const QString& config_key, int min,
                                        int max, int default_value, int scale) {
  auto* right = addSettingRow(parent, title, description);

  auto* spin = new QSpinBox(parent);
  spin->setRange(min, max);
  spin->setFixedWidth(120);
  spin->setValue(
      ConfigManager::instance().get<int>(config_key, default_value * scale) /
      scale);
  right->addWidget(spin);

  connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [config_key, scale](int value) {
            ConfigManager::instance().set<int>(config_key, value * scale);
          });
  return spin;
}

QComboBox* SettingsDialog::addComboBoxRow(QWidget* parent, const QString& title,
                                          const QString& description,
                                          const QString& config_key,
                                          const QStringList& items,
                                          int default_index) {
  auto* right = addSettingRow(parent, title, description);

  auto* combo = new QComboBox(parent);
  combo->addItems(items);
  combo->setFixedWidth(160);
  // 配置存索引，越界时回落到默认项
  const int index =
      ConfigManager::instance().get<int>(config_key, default_index);
  combo->setCurrentIndex((index >= 0 && index < items.size()) ? index
                                                              : default_index);
  right->addWidget(combo);

  connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [config_key](int index) {
            ConfigManager::instance().set<int>(config_key, index);
          });
  return combo;
}

SwitchButton* SettingsDialog::addCheckBoxRow(QWidget* parent,
                                             const QString& title,
                                             const QString& description,
                                             const QString& config_key,
                                             bool default_value) {
  auto* right = addSettingRow(parent, title, description);

  // Fluent 风格滑块开关，而非方框勾选
  auto* toggle = new SwitchButton(parent);
  toggle->setChecked(
      ConfigManager::instance().get<bool>(config_key, default_value));
  right->addWidget(toggle);

  connect(toggle, &QAbstractButton::toggled, this, [config_key](bool checked) {
    ConfigManager::instance().set<bool>(config_key, checked);
  });
  return toggle;
}
