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

// ===== 日志格式开关 =====
// 每项独立控制 pattern 中的一段是否输出，全部变更即时生效
constexpr const char* kConfigLogFmtPrefix = "log/format/";
constexpr const char* kConfigLogFmtTime = "log/format/time";
constexpr const char* kConfigLogFmtLevel = "log/format/level";
constexpr const char* kConfigLogFmtThread = "log/format/thread";
constexpr const char* kConfigLogFmtModule = "log/format/module";
constexpr const char* kConfigLogFmtSource = "log/format/source";
constexpr const char* kConfigLogFmtFunc = "log/format/func";
constexpr const char* kConfigLogFmtMessage = "log/format/message";

// 日志格式默认开关
constexpr bool kConfigLogFmtDefaultTime = true;
constexpr bool kConfigLogFmtDefaultLevel = true;
constexpr bool kConfigLogFmtDefaultThread = false;
constexpr bool kConfigLogFmtDefaultModule = true;
constexpr bool kConfigLogFmtDefaultSource = false;
constexpr bool kConfigLogFmtDefaultFunc = false;
constexpr bool kConfigLogFmtDefaultMessage = true;

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
