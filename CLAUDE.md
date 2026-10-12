# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述
这是一个基于Qt/C++的学习研究项目，项目采用CMake构建系统，支持windows和WSL2 Ubuntu Focal(验证过)。

## 常用命令
### 构建项目
```bash
# 全量构建（默认 debug 64位）
scripts/build_ninja.bat

# 全量构建 32 位 debug
scripts/build_ninja.bat -a x86

# 仅构建主程序
scripts/build_ninja.bat -t debug -m demo_app

# 构建 + 部署 + 打包安装程序
scripts/build_ninja.bat -t relwithdebinfo -m demo_app -p

```
- 自动配置VS2019 x64编译环境
- 使用Ninja生成器构建
- 工具链用仓库自带的那几份：`3rdparty/cmake/bin/cmake.exe` 与 `3rdparty/ninja.exe`（要求 ≥ 1.10.0，不用 VS2019 自带的那份），预设已固定指向它们
- 界面有两条路线，`APP_UI_ROUTE` 二选一（默认 `qss`：项目层样式表 + SARibbon；`fluent`：FluentUI3Style + Frameless，不加载 QSS）。fluent 要独立构建目录：`cmake -S . --preset ninja-relwithdebinfo-x86 -B build/ninja-relwithdebinfo-x86-fluent -DAPP_UI_ROUTE=fluent`。设计与决策见 `docs/plan/界面双路线与FFT选型设计.md`
- 构建输出目录：`build/ninja-debug/`（debug）、`build/ninja-relwithdebinfo/`（relwithdebinfo）、`build/ninja-release/`（release）
- 支持两种参数模式：旧模式（位置参数 `build_ninja.bat debug deploy`）和新模式（显式参数 `-t <type> -m <target> -d/-p`），详见 `scripts/build_ninja.bat -h`
- 可执行文件输出到：`build/ninja-debug/bin/`

注意：编译验证统一用 `scripts/build_ninja.bat -t relwithdebinfo -a x86`（32 位）——产物里有 32 位目标，只在 64 位下验过不算数；也别每次换配置，一条口径才好比对。

## 代码架构
项目采用分层、模块化架构设计，`src/` 下每个子目录对应一个独立的 CMake 构建目标。
模块划分与依赖关系可直接从 `src/` 目录结构及各子目录 `CMakeLists.txt` 中看出，此处不再赘述。



## 项目规则
###  Google C++ 代码风格

遵循 Google C++ Style Guide 核心规则：

| 类别                           | 规则                                                                                                                                 |
| ------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------ |
| **命名 — 类型/类/结构体/枚举** | `PascalCase`                                                                                                                         |
| **命名 — 函数**                | `PascalCase`                                                                                                                         |
| **命名 — 变量**                | `snake_case`                                                                                                                         |
| **命名 — 成员变量**            | `snake_case_`（尾下划线）                                                                                                            |
| **命名 — 常量**                | `kPascalCase`                                                                                                                        |
| **命名 — 宏**                  | `ALL_CAPS`                                                                                                                           |
| **文件命名**                   | `.h` / `.cpp`，`snake_case`，如 `config_manager.h`                                                                                   |
| **头文件保护**                 | `#define` 宏，格式 `PROJECT_PATH_FILE_H_`（禁止 `#pragma once`）                                                                     |
| **包含顺序**                   | 1) 关联头文件 2) C 标准库 3) C++ 标准库 4) Qt 库 5) 项目内头文件，每组空行分隔                                                       |
| **行长度**                     | 不超过 80 字符（注释/字符串放宽至 100）                                                                                              |
| **缩进**                       | 2 空格，无 Tab                                                                                                                       |
| **指针/引用**                  | `*` / `&` 跟在类型后：`int* p`、`const QString& s`                                                                                   |
| **智能指针**                   | 优先 `std::unique_ptr`，共享所有权用 `std::shared_ptr`，禁止裸 `new/delete`                                                          |
| **禁用特性**                   | 禁止 `#pragma once`（用宏保护代替）；禁止 `using namespace std`；禁止 RTTI（`dynamic_cast`）；避免 C 风格转型（用 `static_cast` 等） |
| **注释**                       | 类/函数用 `//`，头文件中声明处写注释，实现文件不重复；避免写"做了什么"，写"为什么这么做"                                             |
| **constexpr/const**            | 编译期常量用 `constexpr`，运行期只读用 `const`                                                                                       |

- 所有生成的代码需要遵循clang-format的Google C++风格规范
- for语句，如果只有一行，也请加上`{}`
- if语句，如果只有一行，也请加上`{}`
- Qt 界面样式禁止在 C++ 代码中通过 `setStyleSheet` 硬编码
  - 所有样式统一写入 QSS 文件中：本仓只维护**项目层** `src/app/resources/styles/default.qss`（**只写挂在 `#objectName` 上的规则**），由 `src/app/main.cpp` 启动时加载。原先的**底座层**（类型/伪状态/子控件/属性选择器规则，原 `src/style/` 模块 `demo_style`）已迁往 `qt-libui` 仓库，由其 `ApplyStyleSheet()` 负责拼接与加载
  - 规则挂哪一层看**控件身份**：挂在类型上的是"哪一类控件"（`QLabel[role="runFailure"]`），归 qt-libui 的底座层（要动它去那边改）；挂在 `#objectName` 上的是"哪一个控件"（`#statusBackend[backend="fake"]`），归本仓项目层。所以项目层不是"只写裸 objectName"，而是"只写**挂在 objectName 上**的规则"；裸写一个 `[backend="fake"]` 不锚定任何控件，仍属底座层的范畴
  - 定位控件按三档优先级：**类型 + 伪状态 + 子控件** → **动态属性**（`setProperty("role", "primary")` 配 `[role="primary"]`；运行中改属性后要 `unpolish` + `polish`）→ `#objectName`（仅项目层，且仅当确实无法用属性表达时）。`objectName` 是 funq 控件路径与 QtTesting 录制的锚点，不要拿它做样式角色
  - 对于QPushButton或者QToolButon而言，如果明确不需要对外观定制，用QPushButton,否则用QToolButton
  - 如果 QSS 必须使用 Type Selector、Descendant Selector 或 Child Selector 定位带命名空间的 Qt/C++ 类，必须写 Qt 元对象系统使用的完整命名空间选择器：将 C++ 命名空间中的 `::` 替换为 `--`
- **禁止在代码中使用 emoji 字符**（包括 `\xF0\x9F...` 等字节转义或直接粘贴的 emoji）
  - 替代方案：统一使用 `src/app/resources/icons/svg/` 下的 SVG 图标，直接 `QIcon(":/resources/icons/svg/<name>.svg")` 加载（新图要在 `res.qrc` 里登记）；缺图时新增 SVG 而非用 emoji
- **本项目不需要使用命名空间**：小Demo项目，命名空间没啥用处


### Git 提交信息规范
```txt
<type>(<scope>): <subject>

[body]

[footer]

## Type（必选其一）
- `feat` - 新功能
- `fix` - 修复bug
- `docs` - 文档
- `style` - 代码格式（不影响运行）
- `refactor` - 重构
- `perf` - 性能优化
- `test` - 测试
- `build` - 构建/依赖
- `ci` - CI配置
- `chore` - 杂项
- `revert` - 回滚

## 规则
1. `subject`：祈使句、现在时，不超过50字，**中文描述**，结尾不加句号
2. `scope`（可选）：模块名，如 `auth`、`api`
3. `body`（可选）：解释为什么，每行≤72字符
4. `footer`（可选）：`Closes #123` 或 `BREAKING CHANGE: 说明`
```

### 其它规则
必须严格遵守以下规则：
1. 所有C++均使用C++17的所有特性，C语言代码使用C11特性。
2. 每次代码改动完成后都**必须**单独一行说一句 `我超威😁`
3. 所有生成的C++代码**必须**遵循Google C++ Style Guide，Qt界面代码需将UI初始化和信号槽连接分离到`initUi()`和`initSignals()`函数
4. 所有的Markdown文档**必须**遵循Google文档风格指南
5. 代码设计需**必须**遵循`SOLID`原则
6. 所有会修改git仓库的命令必须经过我的确认，使用查看类git命令（git status、git log等）的没有限制
7. 文档统一存放在`docs/`目录下，按意图放入到不同的目录中
8. 如果在操作过程中需要编写临时脚本，**禁止放在系统的TEMP目录中**，必须放在工作区的`temp`目录下。
9. 回答问题的语气**禁止**阿谀奉承，不用故意谄媚，只说问题的解决办法和思路，切实中肯，言简意赅。不要每次改动之后都给我说百分百没问题！
10. 使用spdlog编写日志的时候，**尽可能**使用我封装好的Logger类及其宏定义
11. 关于脚本：
    - 编写用户使用的脚本，请使用`powershell`或者`Command Prompt`，Linux下使用`Bash`。尤其是CMake的模板文件`xxxx.ps1.in`,`xxxx.bat.in`,`xxxx.sh.in`
    - CMake使用的模板文件如`xxxx.ps1.in`,`xxxx.bat.in`,`xxxx.sh.in`中的注释，尽量不要用中文，因为在windows上即使脚本本身是UTF-8编码+脚本内 `chcp 65001`，依然有问题，Linux下如果没有安装中文字体，一样有问题。
    - 编写AI Agent工具使用的脚本(这些脚本通常不纳入源代码管理，不被用户直接调用)，请使用 `pwsh` 的语法来编写脚本，若尝试了三次`pwsh`脚本不行，请使用 `python`脚本。
    - python脚本则按照它自己的规则弄。不受`bat`、`ps1`、`sh`脚本的约束。
12. clang-tidy检查在开发机器上太慢了，除非用户明确提示，不然AI Agent默认不做clang-tidy检查。
13. 改动完成代码，编译无误之后，**禁止**直接提交代码！除非是使用git worktree方式。
14. **严禁**中代码中使用QObject::tr("")或者在QObject的派生类中使用 tr("")中文。
15. git提交规则：
   - 能使用中文的描述必须使用中文
   - 当你生成 git commit 信息时：
   - 严禁出现 Co-Authored-By 信息 以及 Co-Authored-By: claude code 助手等相关信息
   - 不使用默认的 Claude 署名
   - 提交信息的开始和结束不能有 @ 符号
16. **头文件顺序有要求的，必须要在包含头文件的地方关掉clang-format**，不然容易被clang-format自动调整头文件顺序导致出问题
17. **发布一律用 `relwithdebinfo` 配置**（`scripts/build_ninja.bat -t relwithdebinfo -p`），不要用 `release`。Release 不带 `/Zi`、构建不产 PDB，崩溃日志的调用栈只能给出裸地址。每次发布的 exe 与对应 PDB 必须一并归档，否则 minidump 事后无法解析。详见 `docs/guide/发布指南.md` 与 `docs/guide/构建配置与崩溃排查.md`
18. 需要从头构建时，**优先只删构建目录下的 `CMakeCache.txt`，不要删整个构建目录**——引入 crashpad、FluentUIStyle 这些之后构建目录是 700 MB 级，全量重编很慢。但删之前要确认没有手工 `-D` 覆盖（例如 `build/ninja-debug-x64` 里的 `APP_CRASH_BACKEND=builtin`）：这类覆盖**只存在缓存里**，删了就静默回到默认值；想长期保留的覆盖写进 `local.cmake`（每次 configure 重新生效，且不入库）。另注意删缓存虽比重编便宜，仍会重跑编译器探测、`find_package`、qmsetup 的 configure 期安装与 crashpad 的 configure
19. **改了 `CMakeLists.txt` 里某个 `option()` 的默认值后，已有构建目录不会跟随**（缓存值优先，且没有任何提示）。判据：改了没生效先 `grep <变量名> <构建目录>/CMakeCache.txt`；要让新默认值生效，用 `cmake -B <构建目录> -U <变量名>`（需 vcvars 环境）或删掉那条缓存项——手工删时注释行必须一起删（注释后面跟空行会让 CMake 报 `Parse error ... Offending entry`）
20. 代码注释必须言简意赅。只写意图，不要写emoji，也无需引用docs目录下的文档
21. **提交前，改动的 C++ 文件必须先过 clang-format**（用仓库自带的 `3rdparty/clang/clang-format.exe`，或直接跑 `scripts\clang_format.bat`——它默认就处理改动过的文件；不在 PATH 上）。仓库风格（`.clang-format`）与手写折行不一致时，人工编辑保存会产生纯格式的噪声 diff。**与规则 12 不冲突**：clang-tidy 默认不跑，clang-format 是提交前的必做项

## 功能开发流程

所有涉及 UI/UX 变更或非纯逻辑的功能开发，必须遵循以下迭代流程：

1. **讨论问题** — 不动代码，先梳理现状、明确问题、对齐目标
2. **形成设计文档** — 写入 `docs/plan/`，包含问题陈述、架构回顾、方案选项及理由、决策记录，文件命名用中文，要见名知意
3. **Subagent 审查设计** — 派 subagent 审查文档的一致性和可行性，标记 🔴/🟡/🔵
4. **逐项修复审查问题** — 对每个问题给出选项和推荐项并说明理由，与用户逐个讨论确认
5. **多轮审查直至无阻塞问题** — 修改后再次派 subagent 审查，反复直到无 🔴 阻塞问题。**最多三轮审查**。
6. **写代码** — 根据最终方案实施
7. **(可选)Subagent 审查代码** — 派 subagent 代码审查。写/改完代码之后编译无误之后询问我是否需要代码审查。
8. **修复直至无问题** — 处理所有审查意见

> **原则**：方案不经过审查不能写代码，代码不经过审查不能算完成。

## 调试和定位问题
1. 该项目是一个Qt的GUI程序。大模型Agent工具无法运行查看GUI，手段之一是通过**增加日志**。我是用了spdlog封装了 `LOG_INFO`、`LOG_DEBUG`等，必要的时候添加上日志！
2. 另外，该项目引入了funq（一个通过共享库注入的TCP传输json命令的python测试框架）。必要时候可以在工作区的`temp`目录下编写python脚本来读取属性或者截图。

## 第三方依赖
第三方库清单与版本见顶层 `CMakeLists.txt` 及各模块的 `CMakeLists.txt`（`find_package` / 静态编译配置），此处不再赘述。