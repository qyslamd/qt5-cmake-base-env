# 单实例检测与崩溃捕获接入 + Linux 构建支持 设计文档

> 状态：已根据第一轮 subagent 审查修订（2026-09-11），审查问题 R1–R4、
> Y1–Y11 全部采纳推荐项。

## 1. 问题陈述

本仓库定位为 Qt5+CMake+Ninja 项目基础模板，当前存在两个缺口：

1. **能力未接入**：`src/core` 已从 etest-demo 移植了单实例检测
   （`SingleInstance`）、崩溃捕获（`crashhandler`）、全局异常处理
   （`GlobalExceptionHandler`）三个模块，但 `main.cpp` 只接了 Logger。
   且移植不完整：`GlobalExceptionHandler.cpp` 因命名空间残留目前
   **编译失败**，两个平台 crashhandler 的平台实现文件未参与构建。
2. **仅支持 Windows 构建**：核心模块存在多处 Windows 专属代码
   （spdlog sink、include 大小写、单实例激活路径），CMake 配置与测试
   环境变量亦按 Windows/MSVC 写死，Linux 无法配置和编译。

参考实现：`D:\trae_workspace\etest-demo`（`src/app/main.cpp` 的初始化序列）。

## 2. 现状回顾

### 2.1 模块现状

| 模块 | 文件 | 参与构建 | 已接入 main | 说明 |
|------|------|----------|-------------|------|
| 单实例 | `core/common/SingleInstance.*` | 是 | 否 | QLocalServer+QSharedMemory 双检测，第二实例转发参数并激活原窗口；**激活实现仅 Windows 分支有效**（见 P9） |
| 崩溃捕获 | `core/crashhandler/CrashHandler.*` | 是 | 否 | 工厂按 `Q_OS_WIN` 选择平台实现 |
| 崩溃捕获 | `core/crashhandler/WindowsCrashHandler.*` | **否** | — | 依赖 dbghelp；源码整体有 `Q_OS_WIN` 守卫 |
| 崩溃捕获 | `core/crashhandler/LinuxCrashHandler.*` | **否** | — | 源码整体包在 `#ifndef Q_OS_WIN` 内（Windows 下为空翻译单元，可编）；依赖 `execinfo.h`（POSIX） |
| 全局异常 | `core/common/GlobalExceptionHandler.*` | **否（编译失败）** | 否 | `GlobalExceptionHandler.cpp:165` 调用不存在的 `etest::core::logger` 命名空间（见 P7） |
| 日志 | `core/logger/*` | 是 | 是 | spdlog 封装；但 `logger.cpp` 无条件包含 Windows 专属 sink（见 P8） |

### 2.2 问题清单

| # | 位置 | 问题 |
|---|------|------|
| P1 | `common/SingleInstance.cpp:10`、`common/GlobalExceptionHandler.cpp:8`、`crashhandler/LinuxCrashHandler.cpp:21`、`src/app/main.cpp:7` | include 大小写与实际文件名不符（`logger/Logger.h` → `logger/logger.h`，`MainWindow.h` → `mainwindow.h`）。Windows 大小写不敏感可编译，Linux 直接失败 |
| P2 | `src/core/CMakeLists.txt` | 源码列表缺 `WindowsCrashHandler.cpp`/`LinuxCrashHandler.cpp`；`dbghelp` 与 `/W4 /WX- /EHsc` 未按平台守卫。注：两个平台实现源文件自身守卫完整，均加入构建亦能编过，平台化列表作为防御性设计保留 |
| P3 | 根 `CMakeLists.txt:8-20` | `local.cmake` 存在即 include 且内容为 Windows 路径语义（`msvc2019_64`）；缺失时非 Windows 分支直接 `FATAL_ERROR`，自动生成分支仅限 `if(WIN32)`。WSL 挂载同一目录配置时会把 Windows 路径喂给 Linux（见 R4） |
| P4 | `tests/CMakeLists.txt:3-4、78-82` | Linux 下 `QT_BIN_DIR`/`QT_PLUGINS_DIR` 未定义，`ENVIRONMENT` 会被拆出空 `PATH=` 与伪环境变量（`;` 是 CMake 列表分隔符，与平台无关）。现状 tests 未注册任何测试，影响为零，但平台化应一并处理 |
| P5 | 根 `CMakeLists.txt:51-60` | `QT_BIN_DIR/QT_PLUGINS_DIR` 推导的非 Windows 分支是 `# TODO`（消费方均为 Windows 专属产物，Linux 需整体跳过） |
| P6 | `CMakePresets.json:2`、`scripts/build_ninja.bat` | presets `version: 8` 需 CMake ≥ 3.26 才能解析；bat 为 Windows 专属，Linux 直接用 cmake/ctest |
| P7 | `common/GlobalExceptionHandler.cpp:165` | 调用 `etest::core::logger::Logger::shutdown()`，全仓库已无 `namespace etest`，两个平台都编不过（etest-demo 移植残留） |
| P8 | `core/logger/logger.cpp:8、81、106` | 无条件 include `spdlog/sinks/wincolor_sink.h`（Windows 专属，内部拉 `wincon.h`），Linux 编译失败；`#ifdef _DEBUG` 为 MSVC 专属宏，GCC/Linux 的 Debug 构建不定义，控制台 sink 会静默消失 |
| P9 | `core/common/SingleInstance.cpp:184-198` | `activateWindow()` 仅 Windows 分支使用 HWND 恢复+置前，非 Windows 只 `emit showApplication()` 且无人连接该信号；`:197` 在分支之外又无条件 `emit` 一次（Linux 双发）。Linux 下第二实例激活路径实际是断的 |
| P10 | `core/logger/logger.cpp:90` | 日志文件名硬编码 `"/FILE.log"`，未随 `applicationName()` 走；与 P11 防呆配套修复 |
| P11 | `src/app/main.cpp:17`、`crashhandler/CrashHandler.cpp:23`、`crashhandler/LinuxCrashHandler.cpp:25、33`、`crashhandler/WindowsCrashHandler.cpp:18` | 占位烙印：`applicationName` 为 `"FILE"`；崩溃文件名前缀 `etest_crash_`；Linux 崩溃日志内 `[ETestStudio]` 前缀及 Windows 侧注释。占位 appKey 不改时，不同模板产物会在同一机器互认单实例 |

已排查**无缺项**的部分（第一轮审查全量核查通过）：`demoes/`、
`3rdparty/QCustomPlot`、`QXlsx`、`auth`（QCryptographicHash）、`config`
（QStandardPaths）均跨平台；`src/app` 的 rc/windeployqt/ISCC/run_app.bat
均已 `if(WIN32)` 守卫；全部源文件 UTF-8 编码无问题。

## 3. 方案

### 3.1 main.cpp 接入

```text
QApplication 构造
→ setOrganizationName / setApplicationName（必须先于单实例检测）
→ applicationName 占位值防呆检查（见下）
→ 单实例检测（第二实例转发参数后退出）
→ ConfigManager → Logger::init → GlobalExceptionHandler::init
→ CrashHandler::create()->init()
→ 主窗口 → 登录 → w.show()
→ setActivationWindow(w.winId()) + 连接 showApplication 信号
→ app.exec
→ 退出：GlobalExceptionHandler::shutdown → Logger::shutdown
```

要点：

- **占位防呆**（配套 D2）：`applicationName() == "FILE"` 时输出 WARNING
  日志，提示模板使用者修改组织/应用名，否则单实例键、日志/配置/崩溃
  目录会与其它模板产物冲突（防呆思路与 ISCC_APP_ID 的 GUID 警告一致）。
- **第二实例**：`connectToExistingInstance(arguments)` 转发命令行后
  `return 0`。首实例收到参数后 Windows 走 `activateWindow()`（HWND
  恢复+置前）；非 Windows 通过 `showApplication` 信号连接到
  `w.showNormal(); w.raise(); w.activateWindow();`（修复 P9，并移除
  `SingleInstance.cpp:197` 的无条件重复 emit）。
- **setActivationWindow 时机**：移到登录成功 `w.show()` 之后。登录阶段
  窗口本就不可见，提前取 `winId()` 无收益；`MainWindow` 栈对象存活至
  `app.exec()` 返回，句柄生命周期安全。
- **登录失败提前返回路径**：与正常退出对称，先
  `GlobalExceptionHandler::shutdown()` 再 `Logger::shutdown()`，避免
  信号处理器/Qt 消息钩子挂到进程退出。
- 崩溃文件名前缀 `etest_crash_` → `crash_`；Linux 崩溃日志内的
  `[ETestStudio]` 前缀一并去除（P11）。

### 3.2 core CMakeLists 与源码修复（P1、P2、P7、P8、P10）

- 修正 4 处 include 大小写。
- `GlobalExceptionHandler.cpp:165` 改为 `Logger::shutdown()`。
- `logger.cpp`：平台分支选择 sink——WIN32 用 `wincolor_sink`，其余用
  `ansicolor_sink`；`#ifdef _DEBUG` 改为 `#ifndef NDEBUG`；日志文件名
  改为随 `applicationName()` 生成。
- 源码列表按平台组织：`CrashHandler.cpp`、`GlobalExceptionHandler.cpp`、
  `SingleInstance.cpp` 始终参与；`WindowsCrashHandler.cpp` 仅 WIN32、
  `LinuxCrashHandler.cpp` 仅非 WIN32（防御性设计，两者自身守卫完整）。
- `dbghelp` 用 `if(WIN32)` 守卫，MSVC 编译选项用 `if(MSVC)` 守卫。

### 3.3 根 CMakeLists 支持 Linux（P3、P5）

- `local.cmake` 逻辑：**仅 `if(WIN32)` 时 include**；不存在时 Windows
  走现有"自动生成 + FATAL_ERROR"流程。非 Windows 不读该文件，使用系统
  Qt 或 `-DCMAKE_PREFIX_PATH` / `-DQt5_DIR` 指定安装位置。
- `QT_BIN_DIR/QT_PLUGINS_DIR` 推导整体收进 `if(WIN32)`（消费方均为
  Windows 专属产物）。

### 3.4 tests 平台化（P4）

`ENVIRONMENT` 仅在 WIN32 下注入 Qt 的 PATH/插件路径；Linux 下 Qt 插件
由系统安装路径自解析，无需注入。

### 3.5 文档同步

`docs/guide/模板使用指南.md` 增加 Linux 构建一节：

- 前置条件：CMake ≥ 3.26（presets v8）、qtbase5-dev、qtsvg5-dev、
  qtserialport5-dev 等（与 `find_package` 组件对应）。
- 命令：`cmake --preset ninja-debug-x64` 配置，build preset 名为
  `ninja-debug-x64-build`，测试 preset 为 `ninja-debug-x64-test`。
- 边界：deploy/package/windeployqt/Inno Setup 仍为 Windows 专属；
  `local.cmake` 仅 Windows 需要（Linux 不读取）。

## 4. 决策记录

| # | 决策点 | 选项 | 结论 |
|---|--------|------|------|
| D1 | GlobalExceptionHandler 是否接入 | a) 接入；b) 只接单实例+崩溃捕获 | **a**。Qt 消息重定向属于"基本日志管理"的一部分 |
| D2 | 单实例 appKey 取值 | a) 硬编码字符串；b) `QCoreApplication::applicationName()` | **b**，配套防呆：检测到占位值 `"FILE"` 时输出 WARNING（理由见 P11 的冲突风险）；`FILE.log` 一并随 applicationName 生成 |
| D3 | 去参考项目烙印 | 范围？ | 崩溃文件名前缀 `etest_crash_`→`crash_`，含 Linux 崩溃日志 `[ETestStudio]` 前缀与 Windows 侧注释 |
| D4 | Linux 支持边界 | a) 仅 configure+build+test；b) 连部署打包一起移植 | **a**。windeployqt/Inno Setup 本就是 Windows 工具链，移植无意义 |
| D5 | Linux 验证手段 | a) WSL 实测；b) 仅代码审查 | **a**。WSL Ubuntu 20.04 已备自编译 CMake 3.31 与 Qt5 开发包，ETestStudio 已在其中编译运行 |
| D6 | GEH 与 CrashHandler 职责边界 | 文档是否明确 | **明确**：致命信号（SIGSEGV/SIGABRT/SIGFPE 等）归 CrashHandler（其注册晚于 GEH，产生覆盖）；GEH 实际贡献为 Qt 消息重定向 + SIGINT/SIGTERM，并非双层防护。写入文档与代码注释，避免模板使用者误解 |

## 5. 验证方式

1. Windows：`scripts/build_ninja.bat -t debug -m demo_app` 全量构建通过；
   configure 阶段模板 GUID 警告仍正常；启动两次程序验证第二实例退出、
   原窗口激活。
2. Linux（WSL）：`cmake --preset ninja-debug-x64` 配置 + 对应 build
   preset 构建通过。
3. Subagent 代码审查通过。

## 6. 遗留说明（不在本次范围）

- `cmake/version.h.in:1` 使用 `#pragma once`，与项目"禁止 pragma once"
  规则冲突，属既有模板文件，另行处理。
- 根 `CMakeLists.txt:48` `find_package` 组件表 `network`/`Network`
  大小写重复，可后续清理。
- `demoes/draw_bagua` 的翻译自定义命令依赖未查找的
  `Qt5_LUPDATE_EXECUTABLE`（两平台同样失效），与本次无关。
- `SingleInstance.cpp:1-3` 的 `_WIN32` 守卫风格与其它文件的 `Q_OS_WIN`
  不一致；本项目 Windows 侧为 MSVC-only，不改。
