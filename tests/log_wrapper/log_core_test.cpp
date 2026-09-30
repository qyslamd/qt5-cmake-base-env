#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include "log_wrapper/log_core.h"

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
