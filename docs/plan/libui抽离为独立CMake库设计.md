# libui 抽离为独立 CMake 库 设计文档

> **后续变更（2026-09-20）**：本仓对 `qt-libui` 的引用方式已从
> `find_package` 改为 **git submodule 源码内嵌**（`external/qt-libui`，
> 顶层 `add_subdirectory`）。因此本文中关于 `local.cmake` 的
> `QtLibUi_Root`、按「架构-构建类型」分目录安装（决策 D3）、CRT 不可混用
> 的约束及相关判断**均已作废**；库侧为此去掉了「假定自己是顶层项目」的
> 三处写法。抽离本身（目录布局、目标名 `qt-libui::libui`、头文件
> `include/qt-libui/` 前缀、SplashScreen 契约）仍然有效，本文保留作为
> 当时的决策记录。

> 状态：已根据三轮 subagent 审查修订（2026-09-14）。
>
> 第一轮：事实错误 3 处（find_package 论据、提交数、行数口径）、技术缺陷
> 2 处（缺 `/utf-8`、`GNUInstallDirs` 位置）、关键遗漏 1 处（gallery 的
> CMakeLists 需重写）、决策变更 1 处（D3）。
>
> 第二轮：阻塞 1 处（gallery 的 `ConfigManager` 依赖）、隐式依赖 1 处
> （`SplashScreen` 的 logo 靠消费方资源，新增 D13）、机制闭环与清单补全。
>
> 第三轮：阻塞 2 处（安装树包含根多一层、x86 预设未覆盖，新增 D3 的
> 双架构分支）、图标资产来源、保护宏统一（新增 D14）、若干清单项。

## 1. 问题陈述

本仓库定位为 Qt5 + CMake + Ninja 项目基础模板。`src/libui` 是本仓库里
**唯一一处超出模板所需**的内容：它是标签栏外观与进度条控件族的绘制练习
成果，39 个源码文件共 3196 行（含 `CMakeLists.txt` 为 40 个文件 / 3261 行），
包含 9 种标签栏外观、4 种进度条、若干自绘部件。

它与模板仓的定位有两点冲突：

1. **体量**：模板的价值是"最小可复用骨架"，而 libui 的多数外观是为研究
   绘制技法而写，模板并不需要 9 种标签栏。
2. **依赖模式**：模板仓对第三方一律 vendored（源码进仓 + `add_subdirectory`），
   `find_package` 只用于 Qt 自身。libui 作为本仓的 `add_subdirectory` 目标，
   使用者无法只取走它。

目标：把 libui 抽成**独立的标准 CMake 库** `qt-libui`，仓库内提供完整的
install / export / find_package 设施；模板仓改为通过 `find_package` 引用它，
从而不再背负这 3196 行，同时保留 demo_app 现有的启动画面与滑块开关。

这与 `挂起事项.md` 中"libui 冻结"并不矛盾：冻结是**停止生长**，本次是
**把已长出的部分搬走**，是减法。搬迁过程中不新增任何控件；接口调整仅
D13 一处，且它是为消除一条**现存**的隐式依赖所必需。

## 2. 现状回顾

### 2.1 libui 现状

| 项 | 现状 |
|---|---|
| 文件 | 40 个（39 个源码 + `CMakeLists.txt`），源码 3196 行 |
| 目标名 | `${APP_TARGET_PREFIX}_libui`，即 `demo_libui`（`src/libui/CMakeLists.txt:1`） |
| 库类型 | `STATIC` |
| 依赖 core | **零**。不引用 `Logger`、`AppIconProvider`、`spdlog`；无自己的 `.qrc`、无 `.ui` |
| 依赖 Qt | 仅 `Qt5::Widgets`（`QtMath` 属 QtCore，随 Widgets 传递） |
| 头文件暴露 | `target_include_directories(PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})`，消费方写 `#include "buttons/switch_button.h"` |
| 命名空间 | 无（遵循项目规则），类名为 `SwitchButton`、`SplashScreen`、`TabBarStyle` 等通用名 |
| 测试 | 无。`tests/` 下只有 `core/crashhandler_test.cpp`，不引用 libui |

**一处容易漏掉的隐式依赖**：`src/libui/splash_screen.cpp:39` 用

```cpp
QIcon(QStringLiteral(":/resources/icons/icon.ico")).pixmap(96, 96)
```

取 logo。而全仓唯一的 `.qrc` 是 `src/app/res.qrc:10`，它提供的
`resources/icons/icon.ico` 对应 `src/app/resources/icons/icon.ico`。

即 `SplashScreen` 依赖的是**消费方的资源系统**，且靠一个全局资源路径名
碰巧对上。就"没有自己的 .qrc"而言它确实自包含，但这条链的存在意味着它
并不真正自包含。见 D13。

**保护宏内部不一致**：19 个头文件中 17 个是 `FILE_LIBUI_*`，另有 2 个仍是
`FILE_APP_*`——`buttons/switch_button.h:1`（`FILE_APP_SWITCH_BUTTON_H_`）
与 `splash_screen.h:1`（`FILE_APP_SPLASH_SCREEN_H_`），是这两个文件从 app
层搬进 libui 时漏改的残留。见 D14。

目录构成：

- `buttons/` — `switch_button`
- `progress_bars/` — `progress_bar_base` 基类 + `circle` / `dot_circle` /
  `hourglass` / `water` 四种
- `tab_appearance/` — `tab_appearance.h` 基类、`tab_bar_style` 代理样式
  + `chrome` / `firefox` / `liquid` / `cat_ear` / `jelly` / `torn_paper` /
  `flame` / `neon` / `battery` 九种外观
- 根目录 — `splash_screen`、`radial_navigator`、`sector_menu`

### 2.2 消费方

| 消费方 | 实际用法 | 抽走的痛感 |
|---|---|---|
| `src/app`（demo_app） | 两处：`main.cpp:17,75` 用 `SplashScreen`；`dialogs/settings_dialog.cpp:14,284` 用 `SwitchButton` | 轻 |
| `examples/libui_gallery` | 全部部件的演示，并**直接编入** `src/app/log_output_panel.*`、`log_filter_bar.*` | 重（见 2.4） |

### 2.3 模板仓的依赖管理模式

- **第三方一律 vendored**：`3rdparty/spdlog-1.17.0/`、`3rdparty/QXlsx-1.5.0/`、
  `3rdparty/googletest-1.17.0/` 均为源码进仓 + `add_subdirectory`。
- **`find_package` 只用于 Qt 自身**：`CMakeLists.txt:157` 的
  `find_package(Qt5 COMPONENTS ... CONFIG REQUIRED)`。仓内其余 `find_package`
  均在 `3rdparty/` 内部，属第三方库自身的写法，不是本仓策略。
- **本机路径统一由 `local.cmake` 承担**：`CMakeLists.txt:92-104` 读取它，
  它设置 `Qt5_DIR` / `QT_DIR`。其中的 Qt 路径是**按指针宽度双分支**的
  （`cmake/local.cmake.in:4-8`，`if(CMAKE_SIZEOF_VOID_P EQUAL 8)`）——
  这一点决定了 D3 必须同样双分支。该文件**不入库**（`.gitignore:248`），
  只有模板 `cmake/local.cmake.in` 进仓。逻辑是
  `if(EXISTS local.cmake) → include()  else → 生成模板 + FATAL_ERROR`。
  **注意：已存在的 `local.cmake` 不会被重新生成**，模板改动不会自动传导。
- **`local.cmake` 只在 Windows 生效**：`CMakeLists.txt:92` 是 `if(WIN32)`。
  Linux 侧走系统 Qt，必要时由 `-DCMAKE_PREFIX_PATH` / `-DQt5_DIR` 指定
  （`:91` 注释、`docs/guide/模板使用指南.md:155-156`）。
- **架构是一等公民**：`CMakePresets.json` 的 6 个 configure preset 中 3 个
  是 x86（`:19,48,77`），`README.md:29` 写明"MSVC 2019，x64 与 x86"，
  `scripts/build_ninja.bat` 支持 `-a x86`。

由此，"通过 `find_package` 引用一个**第三方**外部包"对本仓仍是全新模式。
需要修正一处此前的误判：模板仓**并非**"clone 即可构建"——缺 `local.cmake`
时首次 configure 本就会 `FATAL_ERROR`。因此本次改造带来的不是"从可构到
不可构"，而是"多了一个必须在 configure 前就绪的外部包"。

### 2.4 gallery 的依赖面

`examples/libui_gallery` 围绕"操作 → 日志"这一反馈通道设计：

- `gallery_window.cpp` 有 **9 处 `LOG_INFO` 调用**（另有 1 行注释提到它，
  `:177`），`main.cpp:16,23` 另有 2 处
- `main.cpp:15,24` 有 `Logger::init()` / `Logger::shutdown()`
- `main.cpp:3,14` 还有 `#include "config/config_manager.h"` 与
  `ConfigManager::instance()`。它存在的原因是 `main.cpp:11-13` 的注释所
  说明的顺序依赖——"日志配置是从配置里读的"，即它是为 Logger 服务的
- 头文件注释写明意图："部件的操作结果走 LOG_INFO 输出到日志，既能看到
  交互，也能顺带了解"

依赖链：

```text
gallery → Logger / LOG_INFO → core/logger（logger.h、qt_console_sink.h）+ spdlog
        → ConfigManager     → core/config
        → LogOutputPanel    → logger/log_history_buffer.h
        → LogFilterBar      → logger/log_filter.h
```

**一处澄清**（初稿曾误判，此处更正）：`log_output_panel.*` 与
`log_filter_bar.*` 并非 gallery 专属。`src/app/log_window.cpp:5,23` 内含
`panel_ = new LogOutputPanel(this)`，而 `mainwindow.cpp:12,66` 使用
`LogWindow`；`log_filter_bar.h` 由 `log_output_panel.h:10` 包含。即 demo_app
一直在用这两个文件。gallery 迁走后它们仍留在模板仓，**不是死代码**。

### 2.5 可抽取的历史

| 目录 | 提交数 | 时间跨度 |
|---|---|---|
| `src/libui` | 11 | 2026-09-11 ~ 2026-09-12 |
| `examples/libui_gallery` | 8 | 2026-09-12 |
| **并集** | **12** | 多出 `b879931 feat(examples): libui 演示改为标签页加日志面板布局`，它只动 gallery、未动 `src/libui` |

提交信息质量可用，例如 `feat(libui): 沙漏忙碌态改为周期翻转`、
`refactor(libui): 按部件类别分目录并收编进度条控件族`、
`feat(libui): 进度条族补齐忙碌态、sizeHint 与统一动画`。

## 3. 决策记录

| # | 决策 | 理由 | 代价 |
|---|---|---|---|
| D1 | 仓库名 `qt-libui` | 与现有 `qt-cmake-base-env` 同族命名 | 无 |
| D2 | 模板仓**引用**新库 | 保留 demo_app 的启动画面与滑块开关两处演示 | 推翻初稿的"彻底断开"方案 |
| D3 | 库位置**写入 `local.cmake`**，且**按架构双分支**（`QtLibUi_Path_x64` / `_x86` + `set(qt-libui_DIR ...)`） | 与仓内既有的 `Qt5_12_x_Path_x64` / `_x86` **完全同构**。备选通道里，`build_ninja.bat:202` 是 `cmake -S . --preset %PRESET%`，无 `-D` 透传；`CMakePresets.json` 的 `cacheVariables` 是入库共享内容，写本机路径会污染（`CMakeUserPresets.json` 可行，但同样要人工维护一份本机文件，与 `local.cmake` 相比没有优势） | 使用者需在 `local.cmake` 里多填一行；**Linux 侧不适用**，见 R8 |
| D4 | 新库做成标准 CMake 包（install / export / find_package 齐全） | 用户明确要求"标标准准的 cmake 库" | 需承担包配置的正确性 |
| D5 | **不建测试套件**，只保留一条最小验证 | 桌面 GUI 表现无法自动断言，无法走 CI，Agent 也看不到运行效果 | `install/export` 的正确性仅由最小验证兜底 |
| D6 | gallery 跟库走 | 它是 libui 的唯一展示途径，脱离库无处安放 | 需为它换一套不依赖 spdlog 的反馈通道，并重写其 CMakeLists |
| D7 | gallery 的反馈通道换成纯 Qt 版 | 保住边界：新库只依赖 Qt5::Widgets | 演示效果降级，见 §4.4 |
| D8 | 抽取历史，不用全新初始提交 | 仅 12 个提交，抽取成本极低；这些提交记录了"为什么这么画" | gallery 侧历史引用的部分路径在新仓不存在 |
| D9 | 包名 `qt-libui`，消费方写 `qt-libui::libui` | 与仓库名一致，直白 | CMake 目标名含连字符（合法，但不如 PascalCase 常见） |
| D10 | 头文件安装到 `include/qt-libui/`，搜索根取 `include` | 标准布局；搜索根必须是安装目录的上一级，否则消费方会拼出 `include/qt-libui/qt-libui/xxx.h`。库内部源文件 include **一行都不用改**（相对包含按包含者所在目录解析） | 模板仓两处 include 必须加前缀 |
| D11 | 保持 `STATIC`，导出的 config 带 `find_dependency(Qt5 COMPONENTS Widgets)` | 静态库的 `PUBLIC` 链接依赖会写入导出文件，消费方缺 Qt 目标则配置失败 | 无 |
| D12 | 新仓 `examples/gallery/CMakeLists.txt` **重写**，并自生成运行脚本 | 现有文件依赖模板仓设施（见 §4.6），无法直接搬迁；gallery 的用途就是跑起来看，找不到 Qt DLL 等于没有 | 见 §4.6，比初稿估计的重 |
| D13 | `SplashScreen` 新增 `setLogo(const QIcon&)`，logo 由消费方显式提供；未设置时不绘制 logo | 消除 §2.1 那条隐式依赖——库不该假设消费方提供了某个资源路径名。这是本次唯一一处**接口**调整 | 模板仓 `main.cpp` 与 gallery 各需传入图标 |
| D14 | 新仓头文件保护宏统一为 `QT_LIBUI_<路径>_<文件>_H_` | 新仓是独立项目，`FILE_` 是模板仓的占位前缀，不再适用；且顺手修掉 §2.1 说的那 2 处 `FILE_APP_*` 不一致 | 19 个头文件全改。属纯机械改动，不影响行为 |

`write_basic_package_version_file` 的兼容策略取 **`SameMinorVersion`**，
不用 `SameMajorVersion`：版本为 0.1.0 时 major 为 0，`SameMajorVersion`
意味着任何 `0.y.z` 都满足对 `0.1.0` 的请求，而 0.x 阶段不承诺兼容。

## 4. 新仓设计

### 4.1 目录结构

```text
qt-libui/
├── CMakeLists.txt              project(qt-libui VERSION 0.1.0)
├── src/                        ← 原 src/libui 的 39 个源码，目录结构不变
│   ├── CMakeLists.txt          ← 重写（见 4.3）
│   ├── buttons/  progress_bars/  tab_appearance/
│   └── splash_screen.*  radial_navigator.*  sector_menu.*
├── examples/
│   ├── CMakeLists.txt          ← 重写
│   └── gallery/                ← 原 libui_gallery
│       ├── CMakeLists.txt      ← 重写（见 4.6）
│       ├── run_gallery.bat.in  ← 新增，Windows 运行脚本模板（见 4.6）
│       ├── run_gallery.sh.in   ← 新增，Linux 运行脚本模板（见 4.6）
│       ├── gallery_res.qrc     ← 新增，提供演示用图标（见 4.4）
│       ├── resources/gallery_icon.ico   ← 从模板仓
│       │                                   src/app/resources/icons/icon.ico 复制
│       ├── main.cpp  gallery_window.cpp  gallery_window.h
│       └── gallery_log_panel.* ← 新增，纯 Qt 输出面板（见 4.4）
├── cmake/
│   └── qt-libui-config.cmake.in
├── .clang-format               ← 从模板仓复制，保持同一套风格
├── .gitignore
└── README.md                   ← 构建、安装、消费方式
```

顶层需 `set(CMAKE_AUTORCC ON)`，因为 gallery 带 `.qrc`（库自身仍无 `.qrc`）。

**`resources/gallery_icon.ico` 的来源要交代**：filter-repo 只抽
`src/libui` 与 `examples/libui_gallery` 两个路径，`src/app/resources/icons/icon.ico`
不会跟过去，从模板仓手工复制一份即可。缺这个文件 rcc 会直接失败。

### 4.2 顶层 CMakeLists

```cmake
cmake_minimum_required(VERSION 3.16)
project(qt-libui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)   # gallery 的 gallery_res.qrc 需要

# libui 源文件含中文字符串字面量且无 BOM（如 radial_navigator.cpp 的
# "收起"/"展开"、splash_screen.cpp 的 "正在启动..."）。MSVC 默认按系统
# 代码页解释源文件，zh-CN 下会把这些字面量编成乱码，必须显式指定。
if(MSVC)
    add_compile_options(/utf-8)
endif()

# GNUInstallDirs 必须在 add_subdirectory(src) 之前 include：
# ${CMAKE_INSTALL_INCLUDEDIR} 在 src/CMakeLists.txt 的
# target_include_directories 里被使用，CMake 在命令调用时即展开变量，
# 迟 include 会得到 $<INSTALL_INTERFACE:/qt-libui> 这样的绝对根路径。
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Qt5 COMPONENTS Widgets REQUIRED)

add_subdirectory(src)

option(BUILD_EXAMPLES "Build widget gallery" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# 不写 INCLUDES DESTINATION：安装期包含目录已在 src/CMakeLists.txt 的
# $<INSTALL_INTERFACE:...> 里声明过，两处都写会往导出目标的
# INTERFACE_INCLUDE_DIRECTORIES 里写同一条路径两遍。
install(TARGETS libui
    EXPORT qt-libui-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qt-libui
    FILES_MATCHING PATTERN "*.h")

install(EXPORT qt-libui-targets
    FILE qt-libui-targets.cmake
    NAMESPACE qt-libui::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/qt-libui)

configure_package_config_file(
    cmake/qt-libui-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/qt-libui-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/qt-libui)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/qt-libui-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/qt-libui-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/qt-libui-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/qt-libui)
```

`cmake/qt-libui-config.cmake.in`：

```cmake
@PACKAGE_INIT@
include(CMakeFindDependencyMacro)
find_dependency(Qt5 COMPONENTS Widgets)
include("${CMAKE_CURRENT_LIST_DIR}/qt-libui-targets.cmake")
check_required_components(qt-libui)
```

### 4.3 src/CMakeLists.txt

```cmake
add_library(libui STATIC ${ALL_HEADERS_SOURCES})

# ALIAS 让仓内示例与外部消费方用同一个名字，顺带在构建期就验证了命名。
add_library(qt-libui::libui ALIAS libui)

set_target_properties(libui PROPERTIES AUTOMOC ON)

target_link_libraries(libui PUBLIC Qt5::Widgets)

# 安装树：搜索根是 include，消费方写 <qt-libui/xxx.h>。头文件装在
# include/qt-libui/ 下，搜索根必须是它的上一级——写成 include/qt-libui
# 会让消费方拼出 include/qt-libui/qt-libui/xxx.h，必然找不到。
# 构建树：暴露 src 根，库内源文件与 examples 沿用现有的包含写法。
#
# 约束：库内所有源文件一律用「同目录裸名」包含（如 "progress_bar_base.h"），
# 不得出现跨子目录包含。install 后头文件保持子目录结构，裸名包含按包含者
# 所在目录解析，故安装树同样成立；一旦有人写成 "buttons/switch_button.h"
# 这类从 src 根出发的路径，库自身能编过、消费方必炸。
target_include_directories(libui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(MSVC)
    target_compile_options(libui PRIVATE /W4 /WX- /EHsc)
endif()
```

源文件清单沿用现有 `ALL_HEADERS_SOURCES` 的写法逐条搬迁。原
`src/libui/CMakeLists.txt` 的 `AUTORCC ON` 在本库目标上不带过去——本库无
`.qrc`（gallery 的 `.qrc` 由顶层 `CMAKE_AUTORCC` 覆盖）。

按 D14，19 个头文件的保护宏统一改为 `QT_LIBUI_<路径>_<文件>_H_`，
其中 `FILE_APP_SWITCH_BUTTON_H_` 与 `FILE_APP_SPLASH_SCREEN_H_` 两处
本来就不一致，一并归位。

### 4.4 gallery 的改造

**日志通道**。新增 `gallery_log_panel.*`：用 `qInstallMessageHandler` 把
消息投递到 `QPlainTextEdit`。改造后 `examples/gallery` 不再依赖 core。

- `gallery_window.cpp` 的 9 处 `LOG_INFO("GALLERY", "…{}", x)` 换成
  `qInfo()`——注意 spdlog 的 `{}` 占位符语法在 `qInfo()` 里不存在，要改成
  `<<` 流式拼接
- `main.cpp:16,23` 另有 2 处 `LOG_INFO` 同样处理
- `main.cpp:15,24` 的 `Logger::init()` / `Logger::shutdown()` 删除
- `main.cpp:3,14` 的 `#include "config/config_manager.h"` 与
  `ConfigManager::instance()` **一并删除**——它存在的唯一理由是
  `main.cpp:11-13` 注释所说的"日志配置从配置里读"，Logger 走后无用途。
  这一条在第二轮审查中被单独标为阻塞项：只删 Logger 不删它，第 3 步
  必然编译失败
- `gallery_window.h:15,60` 的 `class LogOutputPanel;` 前置声明与
  `LogOutputPanel* log_panel_;` 成员、`gallery_window.cpp:20` 的
  `#include "log_output_panel.h"`、`:178` 的 `new LogOutputPanel(this)`，
  全部同步改为新的 `GalleryLogPanel`

**改造粒度**。不要按"约 30 行"排期。现有 `src/app/log_output_panel.cpp`
约 200 行，含按级别着色、过滤条、历史缓冲。取舍是：

- **保留**：滚动输出、级别前缀、按级别着色（`qInstallMessageHandler`
  回调的 `QtMsgType` 参数足以区分级别）
- **丢弃**：过滤条（`LogFilterBar` 整块）、历史缓冲（`Logger::init()` 之前
  的日志无法回收）、分类前缀（`LOG_INFO("GALLERY", ...)` 的分类参数在
  `qInfo()` 里没有对应物）

D7 接受这个降级。若实现时发现"连级别着色都难做"，属正常，不必强行补齐。

**Splash logo**。按 D13，`SplashScreen` 新增 `setLogo(const QIcon&)`；gallery
在演示页设置图标，来源是新增的 `gallery_res.qrc`（图标文件从模板仓复制，
见 §4.1）。**`gallery_res.qrc` 必须列进 gallery target 的源文件**，
AUTORCC 才会处理它——漏了不报错，只是运行时图标空白。

此处**不搬运** `log_output_panel.*` / `log_filter_bar.*`，它们留在模板仓
供 demo_app 使用（见 2.4）。

**命名残留**（易漏）：

- 保护宏按 D14 统一
- `main.cpp:9` 的 `setApplicationName(QStringLiteral("libui_gallery"))`
  是否改名随使用者（建议保持 `libui_gallery`，它是这个演示程序的名字）

### 4.5 新仓不提供的东西

- 无测试套件（D5）
- 无**构建入口脚本**：没有 `CMakePresets.json`，也没有 `build_ninja.bat`
  一类的构建脚本
- 无 CI 配置
- 无 `install/export` 之外的分发设施（不打包、不出预编译包）

注意区分：configure 时生成的 `run_gallery.bat` / `.sh` 属**运行脚本**
（落在 exe 旁边，把 Qt 的 bin 加进 PATH），不属上列的"构建入口脚本"。
模板仓也是同样口径（`scripts/build_ninja.bat` 入库，`run_*.bat` 由
`cmake/run_app.cmake` 生成）。

**README 必须交代 Qt 怎么找**：新仓没有 `local.cmake` 机制，而 VS 开发者
命令行只解决编译器、不解决 Qt。README 需写明 `-DQt5_DIR=<Qt>/lib/cmake/Qt5`
或 `-DCMAKE_PREFIX_PATH=<Qt>`，否则使用者照抄命令会在 `find_package(Qt5)`
处卡住。

### 4.6 examples 的 CMakeLists 与运行脚本

现有 `examples/libui_gallery/CMakeLists.txt` 依赖三样模板仓设施，必须重写：

| 现有内容 | 处置 |
|---|---|
| `configure_run_script("run_${TARGET_NAME}")`（宏在模板仓 `cmake/run_app.cmake`） | 不搬那套宏，改为自建，见下 |
| `${APP_TARGET_PREFIX}_libui` | 改为 `libui`（或 `qt-libui::libui`，两者指同一目标） |
| `${APP_TARGET_PREFIX}_core`、`${CMAKE_SOURCE_DIR}/src/app` 包含路径、`APP_LOG_UI_SRC` | 全部删除——新仓无 core，日志面板已换为 4.4 的纯 Qt 版 |
| `ALL_SRC` | 增补 `gallery_res.qrc`、`gallery_log_panel.*` |

**自建运行脚本的真实体量**（比初稿估计的重，别按"约 10 行"排期）：

1. **Qt bin 目录需要自己推导**，约 4 行。模板仓的 `cmake/run_app.bat.in:4`
   用的 `@QT_BIN_DIR@` 由模板仓顶层 `CMakeLists.txt:164-169` 从
   `Qt5::qmake` 的 `IMPORTED_LOCATION` 反推得出，新仓 §4.2 的顶层没有这段。
   `Qt5::qmake` 在只请求 `COMPONENTS Widgets` 时也存在——它由
   `Qt5CoreConfigExtras.cmake` 创建。
2. **不能用 `$<TARGET_FILE:...>`**：生成器表达式只在 generate 期求值，
   而 `configure_file` 的 `@VAR@` 不做求值。若确需，得换 `file(GENERATE ...)`。
   新仓 §4.2 未设 `CMAKE_RUNTIME_OUTPUT_DIRECTORY`，exe 落在该子目录的
   `CMAKE_CURRENT_BINARY_DIR`，因此"`configure_file` 输出到当前二进制目录"
   这条路可行——但这一点必须写明，否则实现时容易踩 `$<TARGET_FILE_DIR:>`
   的坑。
3. **需要两个新模板文件** `run_gallery.bat.in` 与 `run_gallery.sh.in`
   （`configure_file` 必须有源文件），已补入 §4.1。
4. **Linux 版的 `.sh` 要 `chmod +x`**：模板仓 `cmake/run_app.cmake` 是用
   `file(CHMOD ...)` 兜的。漏了不会报错，只是脚本不可执行——静默失败。
5. **别丢 `QT_PLUGIN_PATH`**：`cmake/run_app.bat.in:4-5` 同时设 `PATH` 与
   `QT_PLUGIN_PATH`。官方 Qt Windows 包把 `qt_prfxpath` 烘死，`PATH` 一条
   通常够用，但模板仓特意写了两条，新脚本少一条属无谓降级。

## 5. 模板仓改动清单

| 动作 | 对象 |
|---|---|
| 删 | `src/libui/`（40 文件） |
| 删 | `examples/` 整目录（含 `examples/CMakeLists.txt`；其下只有 `libui_gallery`，共 5 个文件） |
| 删 | 顶层 `CMakeLists.txt:197-200` 的 `BUILD_EXAMPLES` 选项块；`:196` 的注释随之删除 |
| 改 | 顶层 `CMakeLists.txt:193` 的注释 `# src 下的各模块（core / libui / app）`——去掉 libui 字样 |
| 改 | 顶层 `CMakeLists.txt:27-29` 的 `APP_TARGET_PREFIX` 注释（去掉 libui 字样） |
| 改 | `src/CMakeLists.txt`：去掉 `add_subdirectory(libui)` |
| 改 | 顶层 `CMakeLists.txt`：新增 `find_package(qt-libui CONFIG)`，**位置须在 `:157` 的 `find_package(Qt5 ...)` 之后、`add_subdirectory(src)`（`:194`）之前**。**不要用 `REQUIRED`**，否则失败时 CMake 直接终止，后面自定义提示是死代码。改为：`find_package(qt-libui CONFIG)` + `if(NOT qt-libui_FOUND) message(FATAL_ERROR "…先构建并安装 qt-libui，并在 local.cmake 里填 qt-libui_DIR") endif()` |
| 改 | `cmake/local.cmake.in`：新增与既有 Qt 路径**同构**的双分支（D3）：`QtLibUi_Path_x64` / `QtLibUi_Path_x86` 两个前缀变量 + 按 `CMAKE_SIZEOF_VOID_P` 选择 + `set(qt-libui_DIR "${QtLibUi_Path}/lib/cmake/qt-libui")` |
| 改 | **本机 `local.cmake`（不入库，且不会因模板改动而重新生成）**：手工补上同样的双分支。只改 `.in` 模板不够，本机文件必须手工同步 |
| 改 | `src/app/CMakeLists.txt:65`：`${APP_TARGET_PREFIX}_libui` → `qt-libui::libui` |
| 改 | `src/app/CMakeLists.txt:79`：注释"编译选项与 core / libui 保持一致"改为只提 core |
| 改 | `src/app/main.cpp:17`：`#include "splash_screen.h"` → `<qt-libui/splash_screen.h>`；并在 `:75` 附近加 `splash.setLogo(QIcon(":/resources/icons/icon.ico"))`（D13） |
| 改 | `src/app/dialogs/settings_dialog.cpp:14`：`#include "buttons/switch_button.h"` → `<qt-libui/buttons/switch_button.h>` |
| 改 | `src/app/resources/styles/default.qss:587-620`：按 R6 在该段落加一条交叉注释，指明 `#splashContent` 等 objectName 是与 `qt-libui` 的跨仓契约 |
| 改 | `README.md` — **4 处**：`:11` 分层模块、`:46-47` 快速开始第 3 步"配置 Qt 路径"、`:71` 目录树、`:76` `examples/` 说明 |
| 改 | `docs/guide/模板使用指南.md` — **7 处**：`:15` 目标名 `demo_libui`、`:16` 第 4 行"`local.cmake`（仅 Windows）…**本机 Qt 安装路径**"的定性（D3 后它还承载与 Qt 无关的 `qt-libui_DIR`）、`:40` 目录树、`:44` 依赖方向 `app → libui → Qt`、`:52-55` `src/libui` 部件清单段、`:68-70` `examples/` 段（含 `:70` 提到的 `BUILD_EXAMPLES`）、`:120` "新增一个通用控件：放进 `src/libui/`" |
| 改 | `docs/guide/模板使用指南.md:155-156`（Linux 段）：补一句 Linux 下 qt-libui 的注入方式（R8） |
| 改 | `CHANGELOG.md`：**新增**一条移除记录，不改写 `[1.0.0]` 已发布段落 |
| 改 | `docs/plan/挂起事项.md`：第 4 条「gallery 空载 CPU 偏高」的正文随 gallery 迁移到新仓或标记失效；`:83` 的"libui 冻结"条同理 |
| **不动** | `src/app/log_output_panel.*`、`log_filter_bar.*`、`log_window.*` |

改完后要给出一条可读的失败提示：使用者若无 `qt-libui`，configure 应明确
告知"先构建并安装 qt-libui，并在 `local.cmake` 里填 `qt-libui_DIR`"，而不是
抛 CMake 默认的 find_package 报错。

## 6. 执行顺序

顺序不可颠倒：第 5 步起模板仓依赖已安装的库，先改模板仓会得到构建不通的
中间态。

1. **准备工具并抽取历史**。本机未安装 `git-filter-repo`（实测
   `git filter-repo --version` 报 command not found），先装：

   ```bash
   pip install git-filter-repo
   ```

   然后在**原仓的一个 clone** 上抽取，原仓不受影响。注意 `--no-local`：
   从本地路径 clone 得到的不是 fresh clone，filter-repo 会拒绝执行。

   ```bash
   git clone --no-local <原仓> qt-libui-work
   cd qt-libui-work
   git filter-repo \
       --path src/libui \
       --path examples/libui_gallery \
       --path-rename src/libui/:src/ \
       --path-rename examples/libui_gallery/:examples/gallery/
   ```

   `--path-rename` 带尾斜杠的写法合法（手册明示目录可带可不带）。
   抽取后**验收目标为 12 个提交**（11 ∪ 8，见 2.5），不是 11 或 8。
   已验证无提交会因"只改被丢弃的路径"而消失：唯一碰过
   `examples/CMakeLists.txt` 的 `99774b3` 同时改了 `src/libui/*`。

   退出时注意 filter-repo 会移除 `origin` remote。**新仓地址待定**
   （§8 未决），届时自行 `git remote add origin <新仓地址>`。

2. **补齐包设施**。按 4.2 / 4.3 加顶层 CMakeLists、`src/CMakeLists.txt`、
   `cmake/qt-libui-config.cmake.in`、`.clang-format`、`.gitignore`、README
   （含 §4.5 要求的 Qt 查找说明）。按 D14 统一保护宏。

3. **改造 gallery**。按 4.4 换日志面板与 `setLogo`（含复制图标、挂
   `.qrc`），按 4.6 重写两个 CMakeLists 并加两份运行脚本模板，在新仓
   构建通过。

4. **分两个架构各装一次并验证包**。前缀取
   `D:/sb/sdk/qt-libui-x64` 与 `D:/sb/sdk/qt-libui-x86`（可按需自定，但
   必须与第 5 步 `local.cmake` 里的两个变量一致）：

   ```bash
   # x64
   cmake --install build/x64 --prefix D:/sb/sdk/qt-libui-x64
   # x86：按模板仓 scripts/build_ninja.bat -a x86 的既定方式切到 32 位环境后
   cmake --install build/x86 --prefix D:/sb/sdk/qt-libui-x86
   ```

   然后在**一个干净目录**里写一个最小消费程序：

   ```cmake
   find_package(qt-libui CONFIG REQUIRED)
   target_link_libraries(mini PRIVATE qt-libui::libui)
   ```

   ```cpp
   #include <qt-libui/buttons/switch_button.h>
   ```

   实例化一个 `SwitchButton`，配置、编译、链接三步全过。**include 必须
   写成带前缀的形式**——写成裸名会让第 4 步"通过"而问题推到第 5 步才炸。

   这是 D5 说的最小验证，也是唯一能自动断言的环节。**第 4 步的验证前缀
   与第 5 步消费的前缀必须是同一个**，不要用临时目录验证、再用另一个
   路径消费。

   **关于构建配置**：预编译静态库的 CRT 需与消费方一致
   （`CMakePresets.json:16` 钉了 `CMAKE_MSVC_RUNTIME_LIBRARY`）。本次按
   模板仓规则 15 用 `relwithdebinfo` 装库，被 debug 预设消费时会出
   LNK4098 与混合堆——实践上可接受（与今天链接 release 版 Qt 同类）。
   若要严格一致，则需为每个构建类型各装一份，本次不做。

5. **改模板仓**：按第 5 节逐项执行，含本机 `local.cmake` 的双分支补充。

   **Linux 侧另走一条路**（`local.cmake` 在 `if(WIN32)` 内，不生效）：
   配置时显式给对应架构的前缀，例如 x64 下
   `cmake -S . -B build/linux-x64 -G Ninja -DCMAKE_PREFIX_PATH=/opt/qt-libui-x64`
   （WSL 下把 Windows 侧的 `D:/sb/sdk/qt-libui-x64` 换写成
   `/mnt/d/sb/sdk/qt-libui-x64`），或把库装到 `/usr/local`。

6. **两边全量构建**，由用户实际运行 gallery 与 demo_app 确认外观与交互。

## 7. 风险

| # | 风险 | 处置 |
|---|---|---|
| R1 | 抽取后 gallery 历史里引用的 `src/app/log_*` 路径在新仓不存在，历史可读性打折 | 接受。历史的价值在提交信息表达的意图，不在路径可回溯 |
| R2 | 无测试套件，install/export 若配错，只有第 4 步能发现 | 第 4 步是强制的，不通过不进第 5 步 |
| R3 | 模板仓多了一个 configure 前必须就绪的外部包 | README 与 `模板使用指南.md` 写清步骤；`find_package` 失败时给可读报错 |
| R4 | `qt-libui` 的类名无命名空间，作为外部库有撞名风险 | 本次接受（D9）。D10 的 `include/qt-libui/` 目录前缀提供一层隔离，但不解决类名 |
| R5 | 新仓无构建入口脚本，Windows 上需 VS 开发者命令行 | 接受，README 写明 |
| R6 | **QSS 与 libui 的 objectName 构成隐式跨仓契约**：`src/app/resources/styles/default.qss:587-620` 用 `#splashContent`、`#splashLogo`、`#splashTitle`、`#splashStatus`、`#splashProgressBar` 给 libui 的 `SplashScreen` 定样式，对应 `src/libui/splash_screen.cpp:24,35,44,49,55`。搬迁后库侧改 objectName，模板仓的 QSS 会**静默失效** | 本次两仓都不动这些 objectName。按 §5 在 QSS 段落加交叉注释，并在 `qt-libui` 的 README 同步说明 |
| R7 | 只删工作区文件，模板仓的 `git clone` 仍会拿到 libui 的全部历史 | 本次不做历史重写（见 §8）。"不再背负 3196 行"仅对工作区成立 |
| R8 | D3 的通道只在 Windows 生效，Linux 侧需另给 `CMAKE_PREFIX_PATH` | §6 第 5 步给出 Linux 命令；`模板使用指南.md:155-156` 加注 |
| R9 | 静态库跨构建类型混用（relwithdebinfo 的库被 debug 预设消费）会产生 LNK4098 与混合堆 | 接受，见 §6 第 4 步。要严格一致需按构建类型各装一份，本次不做 |

## 8. 不在本次范围

- 命名空间 / 类名前缀（R4）
- `qt-libui` 的测试套件
- 新仓的构建**入口**脚本、CI、预编译包（运行脚本见 §4.6，不在此列）
- 按构建类型（debug / relwithdebinfo / release）分别安装库（R9）
- libui 内部的绘制逻辑改动：本次只搬位置，不改绘制。**接口调整仅 D13
  一处**（`SplashScreen::setLogo`），且是为消除现存隐式依赖所必需
- **模板仓的 git 历史重写**（R7）。若将来要让 clone 下来的人看不到 libui
  历史，需另立事项处理
- 把 `qt-libui` 推送到远程仓库。**未决**：是否推、推到哪个平台、仓库地址
  ——第 1 步的 `git remote add` 需要它

## 9. 已评估未采用的方案

审查提出两条替代路径，记录不采用的理由。

**submodule + `add_subdirectory`**。库独立成仓，模板仓以 submodule 引入。
优点明确：保住"clone 即可构建"，与本仓既有的 vendored 策略一致，且省掉
整套 install/export 与路径注入。

不采用的理由有两条：

1. **目标不同，不是效率之争**。D4 要的是一个可被**任意工程** `find_package`
   的包；submodule 只解决模板仓自己的消费方式。二者不互斥（库仓照样可以
   带 config 文件），但选了 submodule，模板仓就不会去验证 export 是否正确，
   D5 的最小验证也失去依托。
2. **破坏模板仓的主用法**。README:41 的用法是"克隆或复制本仓库为新项目
   目录"，submodule 会让这条主流程带上 `.gitmodules` 与
   `--recurse-submodules` 的负担。这是模板仓的核心卖点，代价比省下的
   install/export 更大。

**`git subtree split`**。git 内置，无 Python 与 filter-repo 依赖，同样能
保留提交信息。

不采用的理由：它一次只能抽取**一个** prefix。本次要同时抽 `src/libui` 与
`examples/libui_gallery`，得抽两次再合并两条无关历史，代价高于装一个
filter-repo。若将来只需抽一个目录，这条是更轻的选择。

## 10. 实现记录（2026-09-14）

实施中发现两处与本设计不符之处，均已按实际情况调整并验证。

### 10.1 CRT 冲突是致命错误，库必须按构建类型分别安装

设计 R9 与 §6 第 4 步设想的是"按架构各装一份"（`qt-libui-x64` /
`qt-libui-x86`），并判断 Debug 消费方链接 relwithdebinfo 库"只会出 LNK4098
与混合堆，实践上可接受"。

**实测证伪**：Debug 消费方（`/MDd`）链接 RelWithDebInfo 的静态库（`/MD`）
时，链接器直接 **fatal error LNK1319**（FAILIFMISMATCH），不是警告。原因是
MSVC 把 RuntimeLibrary 写进 obj 的 `/FAILIFMISMATCH` 标记，跨 CRT 混用在
静态库上无法通过。

**改为**：安装前缀按「架构-构建类型」组合，共 4 份——

```text
D:/sb/sdk/qt-libui/x64-debug
D:/sb/sdk/qt-libui/x64-relwithdebinfo
D:/sb/sdk/qt-libui/x86-debug
D:/sb/sdk/qt-libui/x86-relwithdebinfo
```

Release 与 RelWithDebInfo 同为 `/MD`，共用 relwithdebinfo 一份（本次不单独
安装 Release）。`local.cmake` 的 `QtLibUi_Root` 之下按
`CMAKE_SIZEOF_VOID_P` 与 `CMAKE_BUILD_TYPE` 拼出 `qt-libui_DIR`，实现见该文件
注释。

### 10.2 安装出去的头文件必须带 UTF-8 BOM

设计未涉及。实测发现：库的头文件含中文注释且为 UTF-8 无 BOM，库自身编译有
顶层的 `/utf-8` 兜底，但**消费方不开 `/utf-8` 时 MSVC 按系统代码页（936）
解析，注释字节会破坏语法结构**，表现为 `switch_button.h:11: error C2059:
语法错误:"public"` 之类的连锁报错——而消费方是否开 `/utf-8` 不是库能控制的。

**改为**：给 `src/` 下 19 个头文件加 UTF-8 BOM。MSVC 见到 BOM 即按 UTF-8
解析，与编译选项无关；GCC/Clang 同样识别 BOM。`.cpp` 不加（不被消费方
编译，由库自己的 `/utf-8` 覆盖）。

### 10.3 验证结果

| 项 | 结果 |
|---|---|
| 历史抽取 | 12 个提交（11 ∪ 8），与 §2.5 预期一致 |
| 新仓构建 | x64 / x86 × Debug / RelWithDebInfo 四种组合均通过 |
| 包安装 | 4 个前缀各自 install 成功 |
| 最小消费验证 | x64 与 x86 均可 `find_package` + `<qt-libui/...>` 包含 + 链接 + 运行（rc=0） |
| 模板仓构建 | debug-x64、relwithdebinfo-x64、debug-x86 三种组合均通过（分别链到对应架构与构建类型的库） |
| 库缺失提示 | 把 `QtLibUi_Root` 指向不存在的路径，configure 输出自定义的可读提示而非 CMake 默认报错 |
| 未做 | relwithdebinfo-x86 组合未单独验（逻辑与 debug-x86 同）；GUI 实际效果由使用者运行确认 |
