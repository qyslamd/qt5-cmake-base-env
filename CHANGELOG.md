# 更新日志

本文件记录各版本的主要变更。

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循[语义化版本](https://semver.org/lang/zh-CN/)。版本号由 git
派生——tag 提供前三段、提交数提供 build 号，详见[发布指南](docs/guide/发布指南.md)。

## [Unreleased]

### 新增

- `.gitattributes`：钉死行尾约定。`.bat`/`.bat.in` 为 CRLF、`.sh`/`.sh.in`
  为 LF。此前该行为完全依赖各开发者本机的 `core.autocrlf`，而它不入库
- `.clang-tidy`：静态分析规则，与 `.clangd` 配合在编辑器内实时提示
- `README.md`：仓库入口，含特性、环境要求、快速开始与文档索引
- 发布流程文档与 `CHANGELOG.md`

### 变更

- 编译告警级别统一到 `/W4`：`demo_app` 此前用 MSVC 默认的 `/W3`，
  测试目标则未设置
- 文件名改为 snake_case，并清理残留的 `ETEST_` 前缀头文件保护宏
- 全量 `clang-format` 格式化，并登记到 `.git-blame-ignore-revs`
- 编辑器与静态分析配置：`.vscode/` 纳入仓库、新增 `.clang-format`、
  configure 时生成 `.clangd`

### 修复

- 构建脚本中 `ETest_CMake_Path` 是死变量（从未赋值），展开后往 PATH
  追加空条目；Windows 上空条目意味着搜索当前目录
- Debug 配置此前可以打包，但其产物依赖调试版 C 运行时（不在 VC++
  可再发行包内），在用户机器上无法启动。现于打包前拦截

## [1.0.0] - 2026-09-11

首个正式版本，确立模板形态。

### 新增

- **分层模块**：`src/core`（无 UI 依赖）/ `src/libui`（纯 Qt 控件）/
  `src/app`（界面骨架），依赖方向单向，`libui` 可整包复用
- **构建设施**：CMake + Ninja + CMakePresets（x64/x86 × debug /
  release / relwithdebinfo），一键构建脚本
- **版本管理**：由 git 派生，同一提交在任何机器上构建结果一致；
  exe 版本资源中记录提交 hash
- **部署与打包**：windeployqt 部署 Qt 运行库，Inno Setup 生成安装程序
- **符号归档**：按版本与提交归档 exe 与 PDB，崩溃转储事后可解析
- **基础能力**：单实例运行、崩溃捕获（日志 + minidump）、日志系统
  （spdlog 封装，含界面日志面板与运行时格式开关）、配置管理、登录认证
- **单元测试**：googletest + CTest 骨架，含以子进程触发真实崩溃的
  集成测试样例
- **文档**：模板使用指南、发布指南、构建配置与崩溃排查
- **Linux 构建支持**

### 修复

- 打包流程未调用 windeployqt；`make_package` 引用的脚本路径指向构建树，
  而该脚本实际生成在源码树
- 安装包脚本把 `sqldrivers` 等依项目而异的目录写死为必需，缺失即报错
- 安装包的「安装 VC++ 运行库」复选框无人消费，勾选后无任何效果
- `WindowsCrashHandler::setDumpPath` 未规范化尾分隔符，崩溃日志与
  minidump 被写到目标目录的兄弟路径上
- 单实例检测的命名互斥体从未被创建，导致安装脚本的进程检测恒为假
- 日志的控制台 sink 在 Debug 构建下不生效——MSVC Debug 并未定义
  `_DEBUG`，原判断永远为假
- 配置文件名硬编码，未随 `applicationName` 变化

### 变更

- 应用标识（组织名、应用名、安装包 GUID、单实例互斥体名等）集中到顶层
  `CMakeLists.txt`，新建项目只改一处
- 安装包补齐卸载流程：卸载前关闭运行中的程序，可选删除用户数据
