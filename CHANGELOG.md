# 更新日志

本文件记录各版本的主要变更。

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循[语义化版本](https://semver.org/lang/zh-CN/)。版本号由 git
派生——tag 提供前三段、提交数提供 build 号，详见[发布指南](docs/guide/发布指南.md)。

## [Unreleased]

### 新增

- **funq 手动分离运行示例 `examples/funq/`**：两个终端各起一边，把 funq 的
  server 侧与 client 侧摆开——终端 1 起 `demo_app` 并注入 `libFunq`，终端 2
  只连一条 TCP。客户端脚本七步走完「连上 → 认登录界面 → 输错密码断言提示 →
  换对密码进主窗口 → 触发菜单打开设置 → 读写一个数值 → 收尾」，每步打印它
  看到了什么。演示用本仓自己的 `demo_app` 而不是 gallery：控件都有
  `objectName`，路径可从源码推；同时暴露了两个真坑——开模态对话框的动作必须
  `trigger(blocking=False)`，以及 `setParent` 会让 funq 的兄弟序号与添加顺序
  相反（设置对话框那两个没有 objectName 的页面正好撞上）
- **Linux / WSL 构建入口 `scripts/build_ninja.sh`**：与 Windows 侧的
  `scripts/build_ninja.bat` 同一套参数（`-t` / `-m` / `-p` / `-c` / `-h`
  以及旧式位置参数），qt-libui 仓内亦有对应的一份。与 .bat 有两处刻意
  差异：只做本机架构（没有 `-a`，交叉编译不在支持范围内），没有
  windeployqt（没有 `-d`；Linux 侧打包即 `-p` → `make_appimage`）。
  脚本不走 CMakePresets——预设的 `binaryDir` 恰好与 Windows 侧同名，
  WSL 共享同一棵源码树时会直接覆盖对方的 CMakeCache——改用
  `build/linux-ninja-<类型>`
- **funq 注入式 UI 测试接通 Linux / WSL**：包装脚本、注入库暂存与用例接线都
  按平台分了岔——Windows 用 `funq_attach.bat` + `libFunq.dll`（启动后
  `CreateRemoteThread` 注入），Linux 用 `funq_attach.sh` + `libFunq.so`
  （`LD_PRELOAD`，库的构造器先于 `main()` 加载）。`test_gallery_funq` 两侧
  是同一条用例，五个用例在两平台都过
- **Linux 侧 AppImage 打包**：新增 `make_appimage` 目标与
  `scripts/make_appimage.sh`（由 `cmake/make_appimage.sh.in` 生成，与
  Windows 侧的 `make_installer.bat` 对应）。应用标识、版本号、图标全部由
  顶层定制区注入，同 Windows 一样无需改脚本；产出的 AppImage 与未 strip 的
  可执行文件分别落到 `dist/` 与 `dist/symbols/`
- **打包工具随仓库提供**：`3rdparty/appimage-tools/` 内的 linuxdeploy /
  linuxdeploy-plugin-qt / appimagetool（合计约 46 MiB），打包全程无需联网
- `src/app/resources/icons/icon.png`：256×256 的 Linux 图标，由 `icon.ico`
  的 PNG 帧提取而来
- **Linux 崩溃子进程测试**：`tests/core/crashhandler_test.cpp` 新增三条真实
  崩溃用例（SIGSEGV / SIGFPE / SIGABRT），覆盖此前零覆盖的 Linux 崩溃路径；
  崩溃测试骨架（子进程入口与父进程断言）改为两平台共用
- `.gitattributes`：钉死行尾约定。`.bat`/`.bat.in` 为 CRLF、`.sh`/`.sh.in`
  为 LF。此前该行为完全依赖各开发者本机的 `core.autocrlf`，而它不入库
- `.clang-tidy`：静态分析规则，与 `.clangd` 配合在编辑器内实时提示
- `README.md`：仓库入口，含特性、环境要求、快速开始与文档索引
- 发布流程文档与 `CHANGELOG.md`
- **注入式 UI 测试（funq）**：新增 `BUILD_FUNQ` 开关与一套 funq 接线。被测程序是
  qt-libui 的 gallery 示例（`BUILD_LIBUI_GALLERY` 开关，默认随 gallery 编出来），
  测试时把 `libFunq` 注入进程，用 Python 写用例操作控件并断言属性——被测程序
  零侵入。用例在 `tests/gallery_funq/`，已接进 ctest（`test_gallery_funq`）；
  同目录的 `probe_widgets.py` 用来从运行中的进程里查控件路径
- **funq 的 Python 侧脱离 nose**：上游靠 nose 插件读 `funq.conf` 并注册应用配置，
  而 nose 在 Python 3.10+ 已无法运行。现由 `tests/gallery_funq/funq_bootstrap.py`
  用 funq 自带的公开接口补上，`patches/funq/0004` 则删掉那个永远不会被用到的插件。
  前置条件因此简化为「有一个 Python 3.8+ 解释器」，不需要 pip 安装任何包
- 文档新增[funq 注入式 UI 测试](docs/guide/funq注入式UI测试.md)：机制、控件路径的
  查法、两个常见坑（不可见控件的 `wait_active`、状态复位）与排错（残留进程会让
  下次构建删不掉解压目录）
- **UI 测试接入（QtTesting）**：新增 `BUILD_QT_TESTING` 开关（默认 ON，各构建
  类型通用；QtTesting 是静态库，开了会整套链进 exe，准备发布包时可关掉）。
  带该开关的构建在主窗口挂出「测试 → 录制事件/回放事件」菜单；回放走
  `demo_app --play <脚本>`，播完按结果设退出码，可直接接进 ctest。回放模式
  跳过登录：录制入口在主窗口菜单里，录制总是从已登录状态开始，回放需从同一
  状态起步。接线代码在 `src/app/qttesting/`，`tests/app/` 下有一条冒烟用例
- `qttesting_xml.h` 的校验事件回放补齐：上游 `Examples/TestingDemo.cxx` 的源端
  只读 `command` 并把事件类型写死为动作事件，导致录下的校验事件回放时既不带
  命令也不报错。现按 `property` 属性对称解析，还原成 CHECK_EVENT

### 变更

- **设置对话框的两个页面补上 `objectName`**（`settingsPageLog` /
  `settingsPageLogFormat`）。页面原本是裸 `QWidget`，funq 只能退回「类名 +
  同名兄弟的序号」定位，而那个序号与添加顺序**相反**——`setParent` 会把子
  对象插到 `children()` 列表前面，页面又是先 `new` 再 `addWidget` reparent
  进去的，实测「日志」页反而叫 `QWidget-1`，路径无法从源码推
- **funq 不再查找 Qt Quick**（`patches/funq/0006`）：上游把它写成
  `OPTIONAL_COMPONENTS Quick`，但 Qt5 的 `Qt5Config.cmake` 是按包级
  REQUIRED 逐个组件去查找的，根本不看 CMake 为可选组件写下的
  `Qt5_FIND_REQUIRED_<模块>`——于是在只装了 `qtbase5-dev` 的 Linux 上
  直接报错而不是降级。libFunq 里 Quick 相关的代码本就由 `QT_QUICK_LIB`
  保护，去掉这个组件即整体编译掉
- **通用 UI 部件抽为独立库**：`src/libui`（40 文件 / 3196 行）与
  `examples/libui_gallery` 迁出本仓，成为独立仓库 `qt-libui`，本仓改为
  引用外部库（机制随后改为 submodule 源码内嵌，见下条）。目标名由
  `demo_libui` 变为 `qt-libui::libui`，头文件包含改为
  `<qt-libui/...>` 前缀形式
- **qt-libui 改为 submodule 源码内嵌**：`external/qt-libui` 以 git
  submodule 引入，随本仓一次编译，无需预先构建安装。`local.cmake` 的
  `QtLibUi_Root` 与「按架构-构建类型分目录安装」的约定（为规避静态库
  CRT 跨 MD/MDd 混用）随之作废；Linux 侧不再需要额外指定安装前缀。
  克隆时需 `git clone --recursive`，或事后
  `git submodule update --init --recursive`
- `SplashScreen` 的 logo 改由消费方经 `setLogo()` 传入（该库不再假设
  调用方提供了某个资源路径）
- 顶层 `BUILD_EXAMPLES` 选项与 `examples/` 目录一并移除
- `cmake/check_package_config.cmake` 增加可选入参 `PLATFORM`：Windows 与
  Linux 的拒绝/告警理由不同（前者是调试版 C 运行时无法在用户机器启动，
  后者是体积与调试符号的取舍），故分开措辞。省略该入参时行为与原先一致
- `.gitignore` 放行 `3rdparty/appimage-tools/`，`.gitattributes` 将
  `*.AppImage` 标记为二进制
- 编译告警级别统一到 `/W4`：`demo_app` 此前用 MSVC 默认的 `/W3`，
  测试目标则未设置
- 文件名改为 snake_case，并清理残留的 `ETEST_` 前缀头文件保护宏
- 全量 `clang-format` 格式化，并登记到 `.git-blame-ignore-revs`
- 编辑器与静态分析配置：`.vscode/` 纳入仓库、新增 `.clang-format`、
  configure 时生成 `.clangd`

### 修复

- **Linux 侧 AppImage 打包会无限期挂住**：`appimagetool` 产出 AppImage 前要
  嵌入一份 type2 runtime，而这份东西此前没随仓库提供，它便自己去 GitHub 的
  release CDN 拉——那条线路国内常常不可达，且**这个下载没有超时**。表现是
  进程 CPU 为 0、停在 `do_sys_poll`，日志末行是 `Downloading runtime file
  from ...`，看着像卡在压缩，实际是干等（实测约 8 KB/s）。现把
  `runtime-x86_64` 一并入库，`make_appimage.sh` 显式 `--runtime-file` 指过去，
  并在前置检查里拦住缺失的情况——与当初把三个工具入库是同一个决策
- **`dist/` 未被忽略**：打包输出目录此前靠 `*.exe`/`*.pdb` 规则被动覆盖，
  而 AppImage 与无扩展名的 Linux 符号文件不在任何扩展名规则内，打包后会
  污染 `git status`。现整目录忽略
- **Linux 崩溃日志完全未生成**：`LinuxCrashHandler` 在信号处理器里首次调用
  `backtrace()`，而 glibc 的首次调用需要 dlopen libgcc_s 并做内存分配，两者
  都不满足信号安全约束，在崩溃后的上下文里会失败。改为 `init()` 时预热一次
  `backtrace()`。此前该缺陷无测试覆盖，故一直未被发现
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
