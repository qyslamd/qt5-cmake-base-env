#include "config_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include "config_defs.h"

ConfigManager::ConfigManager() {
  // 统一使用 AppLocalDataLocation + "/config"，与 logger 的 logs 目录同根，
  // 保证配置集中存放。数据目录名由组织名/应用名决定，集中在顶层
  // CMakeLists.txt 的 APP_ORG_NAME / APP_APP_NAME（见 cmake/app_info.h.in）
  QString config_path =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
      "/config";
  QDir().mkpath(config_path);
  // 配置文件名随 applicationName 走，避免不同模板产物共用同一份配置
  QString app_name = QCoreApplication::applicationName();
  if (app_name.isEmpty()) {
    app_name = QStringLiteral("app");
  }
  settings_ = std::make_unique<QSettings>(config_path + "/" + app_name + ".ini",
                                          QSettings::IniFormat);
  settings_->setIniCodec("UTF-8");
  initDefaultValues();
}

ConfigManager::~ConfigManager() = default;

ConfigManager& ConfigManager::instance() {
  static ConfigManager inst;
  return inst;
}

void ConfigManager::initDefaultValues() {
  default_values_[QString::fromLatin1(kConfigLogLevel)] =
      kConfigLogDefaultLevel;
  default_values_[QString::fromLatin1(kConfigLogMaxFileSize)] =
      kConfigLogDefaultMaxFileSize;
  default_values_[QString::fromLatin1(kConfigLogMaxFileCount)] =
      kConfigLogDefaultMaxFileCount;
  default_values_[QString::fromLatin1(kConfigLogKeepDays)] =
      kConfigLogDefaultKeepDays;

  default_values_[QString::fromLatin1(kConfigWindowWidth)] =
      kConfigWindowDefaultWidth;
  default_values_[QString::fromLatin1(kConfigWindowHeight)] =
      kConfigWindowDefaultHeight;

  default_values_[QString::fromLatin1(kConfigCrashBackend)] =
      QString::fromLatin1(kCrashBackendCrashpad);
}

QVariant ConfigManager::getDefaultValue(const QString& key) const {
  return default_values_.value(key, QVariant());
}

void ConfigManager::setOverride(const QString& key, const QVariant& value) {
  overrides_.insert(key, value);
}

template <>
QVariant ConfigManager::get<QVariant>(const QString& key,
                                      const QVariant& defaultValue) const {
  // 覆盖层（--set）最优先；然后是已注册的默认值；未注册时才用调用方显式默认值
  const auto it = overrides_.constFind(key);
  if (it != overrides_.constEnd()) {
    return it.value();
  }
  QVariant default_val = getDefaultValue(key);
  if (!default_val.isValid()) {
    default_val = defaultValue;
  }
  return settings_->value(key, default_val);
}

template <>
void ConfigManager::set<QVariant>(const QString& key, const QVariant& value) {
  if (get<QVariant>(key) == value) {
    return;  // 值未变化，不触发更新
  }
  settings_->setValue(key, value);
  emit configChanged(key);
}

void ConfigManager::sync() { settings_->sync(); }

bool ConfigManager::exportToJson(const QString& file_path) const {
  QFile file(file_path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text |
                 QIODevice::Truncate)) {
    qWarning() << "Config export open failed:" << file_path;
    return false;
  }

  QJsonObject root;
  root[QStringLiteral("config_version")] = kConfigVersion;

  QJsonObject window_obj;
  window_obj[QStringLiteral("width")] =
      get<int>(QString::fromLatin1(kConfigWindowWidth));
  window_obj[QStringLiteral("height")] =
      get<int>(QString::fromLatin1(kConfigWindowHeight));
  root[QStringLiteral("window")] = window_obj;

  QJsonObject log_obj;
  log_obj[QStringLiteral("level")] =
      get<int>(QString::fromLatin1(kConfigLogLevel));
  log_obj[QStringLiteral("max_file_size")] =
      get<int>(QString::fromLatin1(kConfigLogMaxFileSize));
  log_obj[QStringLiteral("max_file_count")] =
      get<int>(QString::fromLatin1(kConfigLogMaxFileCount));
  log_obj[QStringLiteral("keep_days")] =
      get<int>(QString::fromLatin1(kConfigLogKeepDays));
  root[QStringLiteral("log")] = log_obj;

  QJsonDocument doc(root);
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();
  return true;
}

bool ConfigManager::importFromJson(const QString& file_path,
                                   bool override_existing) {
  QFile file(file_path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "Config import open failed:" << file_path;
    return false;
  }
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();
  if (!doc.isObject()) {
    qWarning() << "Config import file format error:" << file_path;
    return false;
  }
  QJsonObject root = doc.object();
  int version = root.value(QStringLiteral("config_version")).toInt(0);
  if (version > kConfigVersion) {
    qWarning() << "Config version too new:" << version << "> current"
               << kConfigVersion;
    return false;
  }

  auto import_group = [this, &root, override_existing](
                          const QString& group_name,
                          const QMap<QString, QString>& key_map) {
    QJsonObject group_obj = root.value(group_name).toObject();
    for (auto it = key_map.begin(); it != key_map.end(); ++it) {
      const QString& json_key = it.key();
      const QString& config_key = it.value();
      if (group_obj.contains(json_key)) {
        if (override_existing || !settings_->contains(config_key)) {
          set<QVariant>(config_key, group_obj.value(json_key).toVariant());
        }
      }
    }
  };

  import_group(
      QStringLiteral("window"),
      {{QStringLiteral("width"), QString::fromLatin1(kConfigWindowWidth)},
       {QStringLiteral("height"), QString::fromLatin1(kConfigWindowHeight)}});
  import_group(
      QStringLiteral("log"),
      {{QStringLiteral("level"), QString::fromLatin1(kConfigLogLevel)},
       {QStringLiteral("max_file_size"),
        QString::fromLatin1(kConfigLogMaxFileSize)},
       {QStringLiteral("max_file_count"),
        QString::fromLatin1(kConfigLogMaxFileCount)},
       {QStringLiteral("keep_days"), QString::fromLatin1(kConfigLogKeepDays)}});
  return true;
}

void ConfigManager::resetAllToDefault() {
  for (auto it = default_values_.begin(); it != default_values_.end(); ++it) {
    set<QVariant>(it.key(), it.value());
  }
}

void ConfigManager::resetKeyToDefault(const QString& key) {
  QVariant default_value = getDefaultValue(key);
  if (default_value.isValid()) {
    set<QVariant>(key, default_value);
  }
}
