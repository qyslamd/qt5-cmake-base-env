#ifndef FILE_CORE_CONFIG_CONFIG_MANAGER_H_
#define FILE_CORE_CONFIG_CONFIG_MANAGER_H_

#include <memory>

#include <QHash>
#include <QMap>
#include <QObject>
#include <QSettings>
#include <QVariant>

// 全局配置管理：QSettings(INI) 后端 + 默认值映射 + 变更通知。
// 单线程亲和（GUI 线程）；configChanged 在调用 set 的线程发射。
class ConfigManager : public QObject {
  Q_OBJECT
 public:
  static ConfigManager& instance();
  ~ConfigManager() override;

  ConfigManager(const ConfigManager&) = delete;
  ConfigManager& operator=(const ConfigManager&) = delete;

  // 通用配置读写接口
  template <typename T>
  T get(const QString& key, const T& defaultValue = T()) const;
  template <typename T>
  void set(const QString& key, const T& value);

  // 注入一次性覆盖：get 优先读它（高于 INI 与注册默认值），不落盘、进程内
  // 有效。供命令行 --set 使用——外部驱动（funq 客户端 / 测试脚本）不改用户
  // 的配置文件就能临时换行为。注意 set() 不感知覆盖层，覆盖期间请勿对同键
  // 调 set（写盘判定会被覆盖值干扰）
  void setOverride(const QString& key, const QVariant& value);

  // 导入导出（JSON）
  bool exportToJson(const QString& file_path) const;
  bool importFromJson(const QString& file_path, bool override_existing = true);

  // 立即同步到磁盘
  void sync();

  // 重置
  void resetAllToDefault();
  void resetKeyToDefault(const QString& key);

 signals:
  void configChanged(const QString& key);

 private:
  ConfigManager();
  QVariant getDefaultValue(const QString& key) const;
  void initDefaultValues();

  std::unique_ptr<QSettings> settings_;
  QMap<QString, QVariant> default_values_;
  QHash<QString, QVariant> overrides_;
};

template <typename T>
T ConfigManager::get(const QString& key, const T& defaultValue) const {
  return get(key, QVariant::fromValue(defaultValue)).template value<T>();
}

template <typename T>
void ConfigManager::set(const QString& key, const T& value) {
  set(key, QVariant::fromValue(value));
}

// QVariant 特化：非内联实现
template <>
QVariant ConfigManager::get<QVariant>(const QString& key,
                                      const QVariant& defaultValue) const;
template <>
void ConfigManager::set<QVariant>(const QString& key, const QVariant& value);

#endif  // FILE_CORE_CONFIG_CONFIG_MANAGER_H_
