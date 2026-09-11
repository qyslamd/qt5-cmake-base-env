#ifndef FILE_CORE_CONFIG_CONFIG_DEFS_H_
#define FILE_CORE_CONFIG_CONFIG_DEFS_H_

// 配置版本号
constexpr int kConfigVersion = 1;

// ===== 日志配置组 =====
constexpr const char* kConfigLogLevel = "log/level";
constexpr const char* kConfigLogMaxFileSize = "log/max_file_size";
constexpr const char* kConfigLogMaxFileCount = "log/max_file_count";
constexpr const char* kConfigLogKeepDays = "log/keep_days";

// 日志默认值（0=debug, 1=info, 2=warn, 3=error, 4=fatal）
constexpr int kConfigLogDefaultLevel = 1;
constexpr int kConfigLogDefaultMaxFileSize = 10 * 1024 * 1024;  // 10MB
constexpr int kConfigLogDefaultMaxFileCount = 20;               // 保留20份
constexpr int kConfigLogDefaultKeepDays = 7;                    // 保留7天

// ===== 窗口配置组 =====
constexpr const char* kConfigWindowWidth = "window/width";
constexpr const char* kConfigWindowHeight = "window/height";

// 窗口默认值
constexpr int kConfigWindowDefaultWidth = 1024;
constexpr int kConfigWindowDefaultHeight = 768;

// ===== 登录配置组 =====
constexpr const char* kConfigLoginRememberUsername = "login/remember_username";
constexpr const char* kConfigLoginRememberPassword = "login/remember_password";

#endif  // FILE_CORE_CONFIG_CONFIG_DEFS_H_
