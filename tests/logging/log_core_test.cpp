#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include "logging/log_core.h"

namespace {

// 每次从干净的临时目录开始。故意用多级且不存在的路径，顺带验证核心会建目录
// （rotating_file_sink 自己不建目录，目录不在会被降级路径吞掉）
std::filesystem::path PrepareLogDir() {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "ephad_log_core_test" / "logs";
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  return dir;
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
}

}  // namespace

// 一个用例跑完整周期：这个进程里没有 QCoreApplication / QApplication，
// 链的也只有 ephad_log + spdlog
TEST(LogCoreTest, WorksWithoutQt) {
  const std::filesystem::path log_dir = PrepareLogDir();
  ASSERT_FALSE(std::filesystem::exists(log_dir));

  // Init 之前打的这一条走启动期缓冲，Init 时回放
  LOG_INFO("MAIN", "pre-init-sentinel");

  LogInitOptions options;
  options.log_dir = log_dir.string();
  options.file_name = "log_core_test";
  LogCore::Init(options);

  LOG_INFO("MAIN", "post-init-sentinel");

  // 热更新：ApplyOptions 必须经 spdlog::set_level 遍历 registry，已存在的模块
  // logger 才会跟着变。若实现只改全局变量，warn 这一行仍会写进文件
  LOG_INFO("MAIN", "before-quiet-sentinel");
  LogInitOptions quiet = options;
  quiet.level = LogLevel::kError;
  LogCore::ApplyOptions(quiet);
  LOG_WARN("MAIN", "filtered-warn-sentinel");
  LOG_ERROR("MAIN", "kept-error-sentinel");

  LogCore::Shutdown();

  const std::filesystem::path file = log_dir / "log_core_test.log";
  ASSERT_TRUE(std::filesystem::exists(file)) << "日志目录没建起来，或文件没落盘";
  const std::string content = ReadFile(file);

  EXPECT_NE(std::string::npos, content.find("post-init-sentinel")) << content;
  // 启动期那条要回放出来，并带上前缀
  EXPECT_NE(std::string::npos, content.find("pre-init-sentinel")) << content;
  EXPECT_NE(std::string::npos, content.find("[启动期]")) << content;

  EXPECT_NE(std::string::npos, content.find("before-quiet-sentinel")) << content;
  EXPECT_EQ(std::string::npos, content.find("filtered-warn-sentinel")) << content;
  EXPECT_NE(std::string::npos, content.find("kept-error-sentinel")) << content;
}

// 与 spdlog 内建 logger **撞名**的模块：必须复用它，不能先 drop 再注册。
//
// Init 注册的默认 logger 名字就叫 "default"，而 Qt 的默认日志类别也叫
// "default"（qtMessageHandler 把 context.category 原样当模块名传进来），
// 所以这条分支真机上一定会走到：drop 掉之后 spdlog::default_logger() 成了
// nullptr，此后每个新建的模块 logger 都拿到 nullptr——日志静默丢失、一条都
// 不落盘，而每处调用点看起来都正常
TEST(LogCoreTest, KeepsLoggingAfterDefaultNameCollision) {
  const std::filesystem::path log_dir = PrepareLogDir();

  LogInitOptions options;
  options.log_dir = log_dir.string();
  options.file_name = "collision";
  LogCore::Init(options);

  LOG_INFO("MAIN", "registered-sentinel");
  // 撞名的这一条：真机上由第一条 Qt 警告触发
  LOG_WARN("default", "default-category-sentinel");
  // 撞名之后才第一次出现的模块——正是"一条不落盘"的那一类
  LOG_INFO("LATE_MODULE", "late-module-sentinel");

  LogCore::Shutdown();

  const std::string content = ReadFile(log_dir / "collision.log");
  EXPECT_NE(std::string::npos, content.find("registered-sentinel")) << content;
  EXPECT_NE(std::string::npos, content.find("default-category-sentinel"))
      << content;
  EXPECT_NE(std::string::npos, content.find("late-module-sentinel"))
      << "撞名之后新建的模块 logger 没把日志送出去，日志静默丢了\n"
      << content;
}
