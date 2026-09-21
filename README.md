# Qt CMake 基础模板

基于 Qt5 + CMake + Ninja 的项目基础模板。克隆后只需修改顶层
`CMakeLists.txt` 中的少量变量即可开始新项目；构建、部署、打包、符号归档、
单元测试等设施开箱可用，无需重复搭建。

## 特性

- **定制面收敛**：新建项目只需改顶层 `CMakeLists.txt` 的「项目定制区」
  （5 个变量），其余文件无需改动
- **分层模块**：`core`（无 UI 依赖）/ `app`（界面骨架），依赖方向单向；
  通用 UI 部件见独立库 `qt-libui`（以 submodule 引入，随本仓一起编译）
- **版本号由 git 派生**：tag 提供主版本号，提交数提供 build 号，exe 版本
  资源中记录提交 hash，同一提交在任何机器上构建结果一致
- **一键构建部署打包**：一条命令完成编译 → 归档符号 → windeployqt →
  Inno Setup 生成安装程序；Linux 侧对应 linuxdeploy → appimagetool 生成
  AppImage
- **符号自动归档**：按版本与提交归档 exe 与 PDB，崩溃转储事后可解析
- **基础能力开箱可用**：单实例运行、崩溃捕获、日志系统（含界面日志面板与
  运行时格式开关）、配置管理、登录认证
- **单元测试骨架**：含以子进程触发真实崩溃的集成测试样例
- **编辑器配置随仓库提供**：`.vscode/`、`.clang-format`、clangd 配置生成

## 环境要求

| 项 | 要求 |
|----|------|
| 操作系统 | Windows（主线）；Linux 亦可构建，见模板使用指南 |
| 编译器 | MSVC 2019，x64 与 x86 |
| Qt | Qt 5.15 |
| CMake | ≥ 3.19；使用 `CMakePresets.json` 需 ≥ 3.26 |
| Ninja | 随构建脚本使用 |
| Inno Setup 6 | 生成安装程序，`3rdparty/` 内已附 |
| linuxdeploy / appimagetool | 生成 AppImage（Linux），`3rdparty/` 内已附 |

第三方库随仓库提供（spdlog、googletest、QCustomPlot、QXlsx），首次
configure 时自动解压，无需另行下载。通用 UI 部件库 `qt-libui` 以 git
submodule 放在 `external/` 下，随本仓一次编译，克隆时需一并拉取。

## 快速开始

1. **获取本模板**：克隆或复制本仓库为新项目目录。`qt-libui` 以 submodule
   形式引入，克隆时要一并拉取：

   ```bat
   git clone --recursive <本仓库地址>
   ```

   已克隆但 `external/qt-libui/` 为空的，执行
   `git submodule update --init --recursive` 补齐

2. **修改定制区**：编辑 `CMakeLists.txt` 顶部的「项目定制区」，共 5 项——
   项目名、组织名、应用名、项目主页、目标名前缀。每项的作用见文件内注释

3. **配置本机路径**：首次执行构建脚本时会自动生成 `local.cmake` 并报错
   提示，按提示填入本机 Qt 路径后重跑

4. **构建并运行**：

   ```bat
   scripts\build_ninja.bat -t debug
   ```

   产物在 `build/ninja-debug-x64/bin/`，直接运行该目录下的
   `run_demo_app.bat` 即可（脚本已设好 Qt 运行环境）

5. **跑测试**：

   ```bat
   ctest --test-dir build/ninja-debug-x64 --output-on-failure
   ```

更细致的说明见[模板使用指南](docs/guide/模板使用指南.md)。

## 目录结构

```text
src/
├── core/   无 UI 依赖：配置 / 日志 / 崩溃捕获 / 认证 / 单实例
└── app/    界面骨架、资源与样式
external/   外部源码仓库（git submodule）：qt-libui 通用 UI 部件
cmake/      构建脚本模块（版本、运行脚本、测试宏、符号归档、打包）
scripts/    构建与打包入口脚本
tests/      单元测试
docs/       文档
config/     随构建拷贝到输出目录的配置样例
```

## 常用命令

```bat
:: 构建（默认 debug x64）
scripts\build_ninja.bat -t debug

:: 构建 x86
scripts\build_ninja.bat -a x86 -t debug

:: 仅构建主程序
scripts\build_ninja.bat -t debug -m demo_app

:: 构建并部署 Qt 运行时（本地直接运行 exe 用）
scripts\build_ninja.bat -t debug -d

:: 发布打包：出安装程序与符号
scripts\build_ninja.bat -t relwithdebinfo -p
```

`scripts\build_ninja.bat -h` 可查看全部参数。

Linux 侧（AppImage）：

```bash
cmake --preset ninja-relwithdebinfo-x64
cmake --build --preset ninja-relwithdebinfo-x64-build
# 打 AppImage + 归档未 strip 的可执行文件到 dist/
cmake --build --preset ninja-relwithdebinfo-x64-build --target make_appimage
```

## 文档

| 文档 | 内容 |
|------|------|
| [模板使用指南](docs/guide/模板使用指南.md) | 必改项、目录结构、单元测试、扩展指引、Linux 构建 |
| [发布指南](docs/guide/发布指南.md) | 打 tag、逐架构打包、产物说明、常见问题 |
| [构建配置与崩溃排查](docs/guide/构建配置与崩溃排查.md) | 各构建配置的差异、符号归档与崩溃转储解析 |
| [funq 注入式 UI 测试](docs/guide/funq注入式UI测试.md) | 零侵入的界面回归：注入被测进程、用 Python 写用例、控件路径怎么查 |

## 许可证

GNU Lesser General Public License v2.1，见 [LICENSE](LICENSE)。
