# 内置 Markdown 文档预览设计

## 一、问题陈述

基座里的文档（`docs/guide`、`docs/plan`）目前只能在外部编辑器里看。目标是把
Markdown 预览内置进程序，让文档在应用窗口里直接打开：表格、代码块（带语言
高亮）、相对路径图片、文档间的相对链接跳转都要能用。

已验证的起点是 `D:\trae_workspace\qt-md-renderer-demo`——它用
`md4c + qlitehtml` 跑通了上述全部能力。本设计把那份 demo 收敛成基座里一个
正式模块，按基座的既有惯例接线，并补上单测、两界面路线验证与裁剪说明。

## 二、现状回顾

### 2.1 技术栈与来源

| 组件 | 版本 | 许可 | 来源 | 作用 |
|---|---|---|---|---|
| md4c | 0.5.3 | MIT | `aa-sources/md4c-release-0.5.3.zip` | Markdown → HTML，纯 C，编译只要 3 个源文件 |
| qlitehtml | 官方仓库 HEAD `180e3eb` | Qt 公司（GPL-3.0 + Qt 例外 / 商业双授权） | `git clone http://code.qt.io/playground/qlitehtml.git` | 渲染 HTML 的 QWidget，QPainter 绘制，不依赖 QtWebEngine |
| litehtml | 0.10 | BSD-3-Clause | `aa-sources/litehtml-0.10.tar.gz` | HTML/CSS 排版引擎 |
| gumbo | litehtml 0.10 自带 | Apache-2.0 | 同上 | HTML 解析 |

qlitehtml 官方仓库的 `src/3rdparty/litehtml` 是 git 子模块
（`qt/qttools-litehtml.git`），克隆下来是空目录——所以 litehtml 由本仓单独提供
（0.10 版）。已逐文件 md5 比对：**该子模块快照与上游 litehtml 0.10 逐字节相同**，
不存在 API 分叉。

demo（`D:\trae_workspace\qt-md-renderer-demo`）实测可用的能力：标题 / 列表 /
表格 / 引用 / 行内代码、围栏代码块按语言高亮、相对路径图片、文档间 `.md` 相对
链接跳转、`QFileSystemWatcher` 热重载。

两条与基座有关的结论：

1. **qlitehtml 源码零改动**。demo 对它的两处改动（`Qt::SkipEmptyParts`、
   `QFont::setFamilies`）是给 Qt 5.12 的兼容守卫，官方仓库当前版本本就没有，
   基座是 Qt 5.15.2，原样编译即可。
2. **qlitehtml 是 GPL 类依赖，带一个有条件的例外**。它的 SPDX 是
   `LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0`
   （`src/qlitehtmlwidget.h` 头部，`LICENSES/` 下三份原文齐全）。例外原文
   （Qt-GPL-exception-1.0）的 Exception 2 允许"把本程序与按你自选许可的插件
   结合成可执行文件并按你的条款分发"，**条件是**可执行文件必须附带显著声明、
   向所有用户提供**本程序**（即 qlitehtml，含你的改动）的完整源码；独立模块
   （业务代码）不在其列。闭源静态链接是否恰好落进这个例外，属于法务判断，
   本设计不做结论，只把事实列清（许可登记见
   `docs/guide/第三方依赖补丁.md`）。
   注意它**不是**本仓第一个 GPL 类依赖：QCustomPlot 是 GPL-3.0-or-later，早已
   链进 demo_app 并用于 `examples/fft_demo`（审查时把"本仓从未有 GPL 类依赖"
   当成了事实，落地前逐个核对才发现——QtTesting 确实是 Kitware 的
   BSD-3-Clause，但 QCustomPlot 不是宽松许可）。所以**这一条仍需单独拍板**
   （见 §七），只是它并非开先例，而是与既有依赖同类。

### 2.2 本设计要遵循的基座惯例

- `src/` 下每个子目录一个 CMake 目标，对外头文件不穿透第三方类型
- 3rdparty：压缩包放 `3rdparty/`，在 `cmake/extract_dependencies.cmake` 的
  `DEPENDENCY_ARCHIVES` 登记，**解压目录名 = 包名去掉后缀**
- **解压产物不入 git，改上游源码留不住**：要改就写 `patches/<库>/NNNN-*.patch`
  并在 `cmake/patch_dependencies.cmake` 里登记（附一句"为什么"）。上游有开关
  可用 option、能用调用方变量绕开的，就不打补丁
- 构建开关集中在顶层「构建开关区」，用 `option()`
- 静态库自带 qrc 时必须 `Q_INIT_RESOURCE`（见 `src/style`、`src/tour` 的写法）
- 单测放 `tests/<模块>/`，用 googletest；AI 看界面用 funq 按 oid 抓单个控件

## 三、决策记录

以下三条由使用者在讨论阶段拍板：

| 决策项 | 结论 | 说明 |
|---|---|---|
| 形态 | 控件 + 独立查看器窗口 | 文档仍从磁盘读，不进 qrc，改完即看 |
| 渲染引擎 | md4c + qlitehtml | 表格 / 图片 / CSS / 内链齐全，效果与 demo 一致 |
| 代码高亮 | 沿用手写规则表，扩语言 | 不引 KSyntaxHighlighting，语言表按仓库文档实际用到的补 |

补充决策（本设计提出，理由随附）：

| 决策项 | 结论 | 理由 |
|---|---|---|
| 文档来源 | 磁盘文件 | 打包进 qrc 会让"改文档要重新构建"，与"文档直接内部打开"相悖；模板使用者裁剪时也不必动资源 |
| 内链策略放哪 | 窗口，不在控件 | 控件只抛 `LinkClicked(QUrl)`，策略（本地 `.md` 自开、其余交系统浏览器）归窗口；控件因此无副作用、可单独复用 |
| 界面路线 | 两条路线共用一份实现 | 查看器是独立顶层窗口，用 QLiteHtmlWidget 自绘，不经 QSS/Ribbon/Fluent，无需分叉 |
| 暗色主题 | 不做 | 基座当前没有暗色主题，先给一份浅色 CSS |
| 目录树 | 不做 | 一窗一篇文档即可满足"打开自己的文档"；目录导航属于编辑器的事，YAGNI |

## 四、设计

### 4.1 模块划分

新模块 `src/markdown/`，目标名 `${APP_TARGET_PREFIX}_markdown`（`demo_markdown`），
静态库。依赖方向单向：`src/markdown → {Qt5::Widgets, qlitehtml, md4c}`、
`src/app → demo_markdown`。

| 文件 | 职责 | 依赖 |
|---|---|---|
| `code_highlighter.{h,cpp}` | 纯函数：源码文本 → 带 `<span class>` 的 HTML 片段 | 无（只用 Qt Core） |
| `markdown_to_html.{h,cpp}` | 纯函数：Markdown → 完整 HTML 正文；调 md4c，再对代码块做高亮后处理 | md4c、`code_highlighter` |
| `markdown_view.{h,cpp}` | 控件：加载文件 / 设置文本，套内置 CSS，装资源回调与热重载，抛链接点击信号 | `markdown_to_html`、qlitehtml、Qt5::Widgets |
| `markdown_window.{h,cpp}` | 查看器窗口：打开文件对话框、拖拽、内链跳转策略、窗口标题 | `markdown_view` |
| `markdown_res.qrc` | 内置 CSS 与（如需的）图标 | — |
| `resources/markdown_default.css` | 默认排版样式 + 高亮配色类 | — |

界面层与渲染层的分界在 `MarkdownView`：它对外只暴露 Qt 类型，qlitehtml 的
头文件不出现在公开头里（成员用前向声明 + 在 .cpp 里构造），第三方可整体替换。

### 4.2 接口

```cpp
// src/markdown/code_highlighter.h
QString HighlightCode(const QString& code, const QString& language);

// src/markdown/markdown_to_html.h
QString MarkdownToHtml(const QString& markdown);

// src/markdown/markdown_view.h
class MarkdownView : public QWidget {
  Q_OBJECT
 public:
  explicit MarkdownView(QWidget* parent = nullptr);
  ~MarkdownView() override;
  void LoadFile(const QString& path);  // 基准 URL = 文件所在目录
  void SetContent(const QString& markdown, const QString& base_dir);
  QString current_file() const;
  bool has_document() const;
 signals:
  void linkClicked(const QUrl& url);      // 信号名跟随仓库既有写法（camelCase）
  void loadFailed(const QString& path, const QString& reason);
 private:
  void ApplyDocument();
  QLiteHtmlWidget* html_ = nullptr;  // 前向声明，第三方类型不进出头文件
  QString markdown_;
  QString current_file_;
  QString base_dir_;
  QFileSystemWatcher* watcher_ = nullptr;
};

// src/markdown/markdown_window.h
class MarkdownWindow : public QDialog {
  Q_OBJECT
 public:
  explicit MarkdownWindow(QWidget* parent = nullptr);
  void OpenFile(const QString& path);  // 非模态显示并载入
  void OpenFileDialog();               // 弹对话框选一篇（菜单项与窗口按钮共用）
 protected:
  void dragEnterEvent(QDragEnterEvent* e) override;
  void dropEvent(QDropEvent* e) override;
};
```

`MarkdownWindow` 也设 `objectName`（`markdownWindow`）：funq 的控件路径按
`::` 逐级寻址（`markdownWindow::markdownView`——funq 认的是 objectName，不是
类名），顶层那段若靠类名兜底，改类名
就会悄悄断掉探针。头文件用 `#ifndef FILE_MARKDOWN_XXX_H_` 保护宏，禁
`#pragma once`。

`MarkdownView` 带 `setObjectName("markdownView")`：funq 控件路径要靠它，
也是"看界面用像素"那条规矩的锚点。

### 4.3 数据流

```text
.md 文件 ──LoadFile──▶ 读 UTF-8 文本 ──MarkdownToHtml──▶ HTML 正文
   │                                        │
   │                                  代码块后处理：抽语言标记 → HighlightCode
   ▼                                        ▼
QFileSystemWatcher 盯住            套 markdown_default.css ──▶ QLiteHtmlWidget::setHtml
（变了就重新走一遍）                基准 URL = 文件所在目录（相对图片/链接以它解析）

链接点击 ──▶ MarkdownView::LinkClicked(QUrl)
                └─▶ MarkdownWindow：本地 .md → 本窗口打开；其余 → 系统浏览器
```

### 4.4 样式

一份浅色 CSS 放进 `markdown_res.qrc`，内容以 demo 那版为基础（正文 14px、
标题分级、`pre` 底色、引用左边框、表格描边、`img { max-width: 100% }`），
外加高亮配色类 `.kw / .str / .num / .cmt`——这四个类名是高亮器与 CSS 的
唯一契约，改配色只动 CSS。

CSS 与 QSS 互不干扰：QLiteHtmlWidget 自绘，不读 QSS；两界面路线下外观一致。

### 4.5 入口

| 入口 | 形态 |
|---|---|
| 主窗口动作 | 「打开 Markdown 文档…」：功能区「文件」面板 / 菜单栏「文件」菜单（两形态共用同一批 QAction 的既有做法） |
| 查看器窗口内 | 「打开…」按钮 + 拖拽 `.md` 进窗口 |
| 命令行 | `--doc <文件>`，走 `LaunchOptions`；启动即开查看器，供脚本与 funq 探针使用 |

**没有构建开关**：文档预览是基座的固定能力，`src/markdown` 无条件构建，`app`
无条件链接它（使用者在实施中明确要求撤掉 `BUILD_MARKDOWN_VIEWER`）。要裁掉它
的项目按 `模板使用指南.md`「按需裁剪」那一节整段删——那条路径不靠宏，靠删。

`--doc` 之外的参数语义不变（未知参数仍按 QCommandLineParser 报错退出）。

### 4.6 构建接线

没有开关（见 §4.5）。3rdparty 引入块放顶层，与 SARibbon / QCustomPlot 等处一致
（下面就是实装的样子）：

```cmake
set(CMAKE_SKIP_INSTALL_RULES TRUE)       # litehtml 的 install(EXPORT) 会进 AppDir
add_subdirectory(3rdparty/litehtml-0.10 "${CMAKE_BINARY_DIR}/litehtml")
unset(CMAKE_SKIP_INSTALL_RULES)
target_include_directories(litehtml SYSTEM INTERFACE   # 上游源码，免刷屏
    "${CMAKE_SOURCE_DIR}/3rdparty/litehtml-0.10/include"
    "${CMAKE_SOURCE_DIR}/3rdparty/litehtml-0.10/src")

set(QLITEHTML_LIBRARY_TYPE STATIC)       # 静态：与基座其余第三方一致
add_subdirectory(3rdparty/qlitehtml-180e3eb/src "${CMAKE_BINARY_DIR}/qlitehtml")
set_target_properties(qlitehtml PROPERTIES AUTOMOC ON)  # 上游写在顶层，得补

add_library(md4c STATIC
    "${CMAKE_SOURCE_DIR}/3rdparty/md4c-release-0.5.3/src/md4c.c"
    "${CMAKE_SOURCE_DIR}/3rdparty/md4c-release-0.5.3/src/md4c-html.c"
    "${CMAKE_SOURCE_DIR}/3rdparty/md4c-release-0.5.3/src/entity.c")
target_include_directories(md4c PUBLIC
    "${CMAKE_SOURCE_DIR}/3rdparty/md4c-release-0.5.3/src")
set_target_properties(md4c PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
```

引入块的位置有硬约束：**必须落在 `CMakeLists.txt:227` 的
`set(QT_VERSION_MAJOR 5)` 与 `find_package(Qt5 ...)` 之后**（即现有的 3rdparty
引入段内）。qlitehtml 的 `src/CMakeLists.txt` 用 `Qt${QT_VERSION_MAJOR}::Widgets`
写目标名，这个变量是基座自己钉死的（为 qmsetup 那类第三方钉的，Qt 5.15 自带的
`Qt5Config.cmake` 并不定义它），提前加会解析成不存在的 `Qt::Widgets`。

md4c 不 `add_subdirectory`：它自己的 CMakeLists **改全局编译设定**——无条件重设
`CMAKE_CONFIGURATION_TYPES`、按需覆盖 `CMAKE_BUILD_TYPE`、并把 `CMAKE_C_FLAGS_*`
里的 `/MD` 清掉强推 `/MT`，与基座的运行库设定混用会在链接期炸；顺带还建一个
`md2html` 可执行文件。本仓只要库，自己列三个源文件最省事。

litehtml 的 install 规则用 `CMAKE_SKIP_INSTALL_RULES` 跳过而不是打补丁——本仓对
FluentUIStyle 用过同样的做法，变量只在 `add_subdirectory` 时被继承。
（审查时在"由 qlitehtml 自带分支引入"的配置下测过 install 不外溢，但那一路会
`EXCLUDE_FROM_ALL`；本设计是自己直接引入 litehtml，install 规则照常注册，所以
这道夹子留着。落地时用 `grep 3rdparty <构建目录>/cmake_install.cmake` 复核。）

### 4.6.1 补丁：零

按上游原样编译即可，`patches/` 本轮为空：

- **qlitehtml**：Qt 5.15 下源码无需改动。`QT_VERSION_MAJOR` 由基座顶层钉死
  （见上），不必像 demo 在 Qt 5.12 上那样手动补；`QLITEHTML_LIBRARY_TYPE STATIC`
  走普通变量判断，父作用域设好即生效；`QLITEHTML_BIN_PATH/LIBRARY_PATH` 不定义
  时它的 install 段整段跳过。
- **litehtml 0.10**：build 开关够用（`LITEHTML_BUILD_TESTING`、`EXTERNAL_GUMBO`），
  install 规则走调用方变量绕开。
- **md4c**：只取三个源文件自建目标，根本不走它的 CMakeLists。

后手：若编译期冒出新问题，一律落 `patches/<库>/NNNN-*.patch` 并在
`cmake/patch_dependencies.cmake` 登记，不改解压目录。

入库的压缩包（`3rdparty/`，登记进 `cmake/extract_dependencies.cmake`）：

- `qlitehtml-180e3eb.tar.gz` —— 官方仓库克隆后打包（短 SHA 命名，同
  `qmsetup-4a3ff82` / `crashpad-7b9686b` 的既有惯例），打包时去掉 `.git`
- `litehtml-0.10.tar.gz` —— 直接用 `aa-sources` 那份，顶层目录已是 `litehtml-0.10/`
- `md4c-release-0.5.3.zip` —— 直接用 `aa-sources` 那份，内含顶层目录
  `md4c-release-0.5.3/`，解压器支持 zip

### 4.7 高亮语言表

`HighlightCode` 按语言名选一组规则：关键字集合、注释符、是否支持块注释。

| 语言名 | 关键字集 | 行注释 | 块注释 |
|---|---|---|---|
| `cpp` `c` `h` `hpp` `cc` …（缺省） | C++ 关键字 | `//` | `/* */` |
| `cmake` | CMake 命令（`if`/`foreach`/`function`/`target_link_libraries` …） | `#` | 无 |
| `bash` `sh` | shell 关键字 | `#` | 无 |
| `python` `py` | Python 关键字 | `#` | 无 |
| `json` | `true` / `false` / `null` | 无 | 无 |
| `qss` `css` | 属性名不列，只认注释与字符串 | 无 | `/* */` |

未识别的语言回退到 C++ 规则（与 demo 一致）。四类着色：关键字、字符串、
数字、注释；输出前对 `& < >` 做转义。

两条渲染契约写进实现，别留给后来者猜：

- `MarkdownToHtml` 用 `MD_DIALECT_GITHUB` 解析（表格、删除线、任务列表都在这个
  dialect 里，用默认 flags 会静默丢表格——那正是本设计的硬需求）。UTF-8 输入是
  md4c 的默认（`MD4C_USE_UTF8`），不必自定义宏。
- `LoadFile` **先 `setUrl`（基准 URL = 文件所在目录）再 `setHtml`**：qlitehtml 的
  `setHtml` 会立刻渲染，顺序反了第一帧的相对图片与相对链接会解析错。

### 4.8 错误处理

| 情况 | 行为 |
|---|---|
| 文件打不开 | 视图内显示一行提示（含路径与原因），窗口不关；同时抛 `LoadFailed` |
| md4c 解析失败 | 视图内显示「Markdown 解析失败」 |
| 图片读不到 | 交给 qlitehtml 的既有行为（留空位），不额外处理 |
| 内链指向不存在的文件 | 视为打开失败，走第一行 |

### 4.9 热重载

`MarkdownView::LoadFile` 之后用 `QFileSystemWatcher` 盯住该文件，变化即重载。
编辑器"先删后建"会让 watcher 丢掉该文件，等它落盘再挂回去——而且是**有界轮询**
（每 100ms 一次、最多 5 秒），不是单次延时：单次重试在慢盘、大文件、改名保存的
编辑器上会扑空，扑空之后就再没有机会挂回去，热重载从此静默失效。重载时保留
滚动位置；读不到内容时保留原内容并抛 `loadFailed`（免得存盘瞬间看到空白页）。
监视挂不上（文件不在、系统名额满）同样抛 `loadFailed`，不静默失效。

## 五、验证方案

1. **编译**：`scripts/build_ninja.bat -t relwithdebinfo -a x86`（本仓统一口径）
   与 x64 各一遍，两条界面路线各配一次
2. **单测**（`tests/markdown/`，gtest，须在 `tests/CMakeLists.txt` 注册）：
   - `markdown_to_html_test`：标题 / 表格 / 围栏代码块的语言类 / 行内代码 /
     HTML 转义 / 解析失败兜底
   - `code_highlighter_test`：四类着色、跨行块注释、未知语言回退、`&lt;` 转义
   - `tests/app/launch_options_test.cpp` 补 `--doc` 的解析用例（该测试直接把
     `src/app/launch_options.cpp` 编进去，改了解析就要同步补）
3. **像素**（funq，脚本进 `scripts/funq/probe_markdown*.py`）：

   ```bat
   3rdparty\python-3.8.8-embed-win32\python.exe scripts\funq\run_demo_app_funq.py ^
       -- --set login/enabled=false --doc docs/guide/模板使用指南.md
   ```

   抓 `client.send_command("grab", oid=widget.oid)`，控件路径是
   `markdownWindow::markdownView`（funq 的路径按 `::` 分段、认 objectName，
   没有 `#` 语法）。
   存 PNG 后读图确认中文、标题、表格、代码块着色。三个前置条件：
   - `--set login/enabled=false` 不能省：非回放模式下登录窗会先模态弹出，
     `--doc` 不豁免，探针会停在登录窗
   - 先确认没有残留实例：单实例逻辑会把参数转发给已有实例后 `return 0`
     （只有 `--export-report` 豁免），此时抓不到任何东西却拿到退出码 0
   - x86 靶子必须用 `3rdparty\python-3.8.8-embed-win32\python.exe` 当 runner
4. **交互**：内链跳转（点 `docs/plan` 里的相对链接）、热重载（改文件看窗口）、
   拖拽打开各验一条，同样落到探针脚本里
5. **回归**：全量 `ctest`（含既有的 funq UI 用例与 QtTesting 回放）保持全绿
   ——新模块进了 `demo_app` 的链接，不能只看自己的两条用例

## 六、文档与裁剪

- `docs/guide/模板使用指南.md`：目录结构补 `src/markdown`、可保留的示例补一条、
  按需裁剪补「关开关或删模块 + 两条 3rdparty 包」
- `docs/guide/第三方依赖补丁.md`：登记 md4c / qlitehtml 的版本、来源与
  **无需补丁**的结论（Qt 5.15 下上游原样可用）
- 本设计文档：保留决策记录与验证记录

## 七、风险与开放问题

| 项 | 说明 | 处置 |
|---|---|---|
| qlitehtml 许可（**已拍板：接受，无条件构建**） | 见 §2.1 结论 2；与既有依赖 QCustomPlot 同类，不是开先例 | 使用者定：接受。许可事实登记在 `docs/guide/第三方依赖补丁.md` 的「许可」一节，并在 `模板使用指南.md` 的「可保留的示例」里点明。剩下的选项（换 Qt 原生 `QTextDocument::setMarkdown`、自写 litehtml 容器）留档备查，不采用 |
| qlitehtml 无版本号 | 上游 `project(VERSION 1.0)`，仓库也没打 tag | 用短 SHA 命名包（`qlitehtml-180e3eb`），来源与克隆日期记在补丁文档里；归档 tar.gz 后源码就固定了，HEAD 再漂移也不影响 |
| 上游子模块为空 | 官方仓库的 `src/3rdparty/litehtml` 是 submodule，克隆不含内容 | litehtml 由本仓单独入库（0.10，已验与子模块快照逐字节相同） |
| qlitehtml 静态库的 Windows 命名 | 上游对 Windows 设了 `PREFIX ""` + `SUFFIX "1<ext>"`，静态库产物叫 `1.lib` | 只影响产物名，必要时在本仓 CMakeLists 里改 `OUTPUT_NAME`，不改上游 |
| x86 编译 | litehtml/gumbo 未在 32 位下试过 | 验证方案第 1 条覆盖 |
| 首次构建耗时 | 新增约 6M 第三方源码（qlitehtml 240K + litehtml 0.10 + md4c） | 一次性代价；只在本模块开关打开时编 |

## 八、实施清单

```text
3rdparty/qlitehtml-180e3eb.tar.gz             新增（官方仓库克隆后打包，去 .git）
3rdparty/litehtml-0.10.tar.gz                 新增（源：aa-sources）
3rdparty/md4c-release-0.5.3.zip               新增（源：aa-sources）
cmake/extract_dependencies.cmake              登记三个包
CMakeLists.txt                                3rdparty 引入块（位置须在 227 行之后）
src/CMakeLists.txt                            add_subdirectory(markdown)
src/markdown/CMakeLists.txt                   新增
src/markdown/code_highlighter.{h,cpp}         新增
src/markdown/markdown_to_html.{h,cpp}         新增
src/markdown/markdown_view.{h,cpp}            新增
src/markdown/markdown_window.{h,cpp}          新增
src/markdown/markdown_res.qrc                 新增
src/markdown/resources/markdown_default.css   新增
src/app/mainwindow.{h,cpp}                    「打开 Markdown 文档…」动作与槽
src/app/launch_options.{h,cpp}                --doc
src/app/main.cpp                              启动时按 --doc 开窗
src/app/CMakeLists.txt                        链 demo_markdown
src/app/resources/icons/svg/markdown.svg      新增（动作图标，已在 res.qrc 登记）
tests/CMakeLists.txt                          add_subdirectory(markdown)
tests/markdown/CMakeLists.txt                 新增
tests/markdown/markdown_to_html_test.cpp      新增
tests/markdown/code_highlighter_test.cpp      新增
tests/app/launch_options_test.cpp             补 --doc 用例
scripts/funq/probe_markdown_view.py           新增（像素 + 内链 + 热重载）
docs/guide/模板使用指南.md                     目录结构 / 保留示例 / 裁剪
docs/guide/第三方依赖补丁.md                   依赖登记（版本、来源、无补丁、许可）
```

## 九、审查记录

### 第 1 轮（subagent）

按 🔴/🟡/🔵 分级回报，逐条处置：

| 级别 | 问题 | 处置 |
|---|---|---|
| 🔴 | 把 qlitehtml 的许可说成"与 QtTesting 同类"，事实错误：QtTesting 是 Kitware 的 BSD-3-Clause，本仓从未有 GPL 类依赖，且在 kissfft 段明确回避过 GPL | 已改写 §2.1 结论 2（附例外出处与条件），并把"是否接受"提升为 §七 的待拍板项 |
| 🟡 | `BUILD_MARKDOWN_VIEWER=OFF` 没有接线方案：链接与头文件引用都会断 | 一度按宏门 + `--doc` 告警修补；实施中使用者要求撤掉开关（"这个 markdown 我要定了"），最终**无条件构建**，`app` 无条件链接，裁剪改为"整段删"的文档指引 |
| 🟡 | funq 验证命令按字面跑不起来：路径语法写成 `#markdownView`（funq 只认 `::` 分段）、登录窗会拦截、单实例会把参数转发走 | §5-3 改写：正确路径、`--set login/enabled=false`、残留实例前置检查 |
| 🟡 | 入库压缩包的顶层目录名必须与包名严格对应，否则 configure 期 `FATAL_ERROR`；且原拟用的 `aa-sources/qlitehtml` 是脏副本（3 改 + 4 未跟踪 + 195MB build） | 来源已改为官方仓库克隆（本条自然消解），§4.6 写明打包口径与目录名 |
| 🟡 | 实施清单漏 `tests/CMakeLists.txt` 注册与 `launch_options_test.cpp` | §5-2、§8 补齐 |
| 🟡 | md4c 不 `add_subdirectory` 的理由写错（它没有测试目标），真陷阱是它改全局编译设定（强推 `/MT`、覆盖 build type） | §4.6 改写理由 |
| 🔵 | 3rdparty 引入块要落在 `set(QT_VERSION_MAJOR 5)` 之后 | §4.6 补硬约束与锚点 |
| 🔵 | 只加 `src/` 时 `QLITEHTML_VERSION` 取的是基座版本号 | 落地时在引入块里固定 1.0，避免基座升版改库名 |
| 🔵 | 没点名 `MD_DIALECT_GITHUB`（不写会静默丢表格）、`setUrl` 须先于 `setHtml` | §4.7 前新增"渲染契约" |
| 🔵 | 信号命名应为 camelCase；补头文件保护宏要求 | §4.2 已改 |
| 🔵 | `closeEvent` 里的"记住窗口尺寸"没有存储方案 | 删掉该需求（YAGNI） |
| 🔵 | 交互验证没有对应探针脚本条目 | §5-4、§8 补 `probe_markdown_view.py` |
| — | 审查报告自身的一处事实错误 | 它称"本仓从未有 GPL 类依赖"，逐个核对后发现 QCustomPlot 是 GPL-3.0-or-later 且已链进 demo_app（QtTesting 是 BSD-3 这点它对了）。已据此改写 §2.1 结论 2 |

### 第 2 轮（subagent，代码审查）

审查者对每个结论都做了实测（自己写探针链真实产物跑），逐条处置：

| 级别 | 问题 | 处置 |
|---|---|---|
| 🔴 | `HighlightCodeBlocks` 的正则吞文档：md4c 在 GFM 下放行内联原始 HTML，正文里未闭合的裸 `<pre><code>` 会被一直匹配到后面下一个 `</code></pre>`，中间整段正文被当代码转义+着色 | 正则加 `MultilineOption` + 行首锚 `^`（md4c 的块级代码块必从行首开始）；新增单测 `StrayInlinePreCodeDoesNotSwallowLaterBlocks` 锁住。更彻底的"改用 md4c 回调自己拼 HTML"记在下面"备查" |
| 🔴 | 页内锚点点一下弹资源管理器：qlitehtml 把纯 fragment 解析成"当前基准 URL（是目录）+ 片段"，`toLocalFile()` 得到目录 → 落到 `openUrl` | `OnLinkClicked` 先看 `fragment()`：无路径时只滚锚点（`MarkdownView::ScrollToAnchor`）；跨文件带锚点在载入后滚过去 |
| 🟡 | `LoadFile` 失败后状态不一致：标题写新文件，视图/watcher 停在旧文件 | `LoadFile` 改为返回 `bool`，失败时 `ClearDocument()`（清文件/目录/内容 + 停 watcher）并回错误页；窗口只在成功时改标题与路径标签 |
| 🟡 | "先删后建"的单次 100ms 重试会扑空，热重载此后静默失效 | 改为有界轮询（100ms × 50 次），并检查 `addPath()` 返回值，挂不上就抛 `loadFailed` |
| 🟡 | `SetContent(md, "")` 不重置基准，相对图片仍按上一份文档解析 | `ApplyDocument` 无条件 `setUrl`（空基准显式 `QUrl()`） |
| 🟡 | 三个压缩包未入库（交付即断） | 本就在本次提交范围内，第 1 笔提交补齐 |
| 🟡 | 文档与实装相反：§4.6 还写着开关与"不去 set"的那行；《模板使用指南》写成"唯一一个 GPL 类依赖" | 两处都改了（开关段删掉、改成"又添了一个"，与 QCustomPlot 的登记一致） |
| 🔵 | `MD_HTML_FLAG_SKIP_UTF8_BOM` | 加上（当前调用方不会触发，给直接喂字节的调用方兜底）|
| 🔵 | 重载回页首 | 重载前后保存/恢复竖向滚动值 |
| 🔵 | `linkClicked` 无 scheme 白名单 | 只放行 http/https/mailto 与真实存在的本地文件 |
| 🔵 | `loadFailed` 无接收者 | 窗口接上，把失败原因写进路径标签 |
| 🔵 | 文档里的 funq 路径写成类名 | 改成 `markdownWindow::markdownView`（与脚本一致） |

**备查（本轮不做）**：把"解析自己生成的 HTML"换成 md4c 的 `MD_PARSER` 回调
（`MD_BLOCK_CODE`/`MD_TEXT_CODE`）自己拼 HTML——高亮与转义都在回调里做，
`UnescapeHtml` 与两条正则一起消失，根除同源问题。改动面比现在这版大，等真有
需求再上。

### 实施记录（验证结果）

| 项 | 结果 |
|---|---|
| 上游第三方编译 | litehtml 0.10 + gumbo + qlitehtml（官方 HEAD `180e3eb`）在 Qt 5.15.2 / MSVC 2019 下**零补丁**编过，x86 与 x64 各一遍 |
| 单测 | `test_markdown_to_html`、`test_code_highlighter` 各一条，x86 全过 |
| 全量回归 | x86 `ctest` 14/14 通过（含 funq UI 用例与 QtTesting 回放）；x64 构建通过 |
| 像素 | funq 抓 `markdownWindow::markdownView`：`docs/guide/模板使用指南.md` 的中文正文 + GFM 表格带边框；自造样例里 cpp/cmake/bash 代码块四类着色、行内代码、列表、引用、相对路径图片（PNG 从磁盘加载）均正常 |
| 热重载 | 覆盖样例文件后视图自动刷新（未重启进程） |
| 实施中新踩的坑 | ① qlitehtml 的 `CMAKE_AUTOMOC ON` 写在它顶层 CMakeLists 里，只加 `src/` 会丢 → 补 `set_target_properties(AUTOMOC ON)`；② `Q_INIT_RESOURCE` 放进匿名命名空间会绑到匿名空间上、链接期找不到 rcc 生成的函数（仓库注释里早有这条）|

按原判断写成补丁的 `0001-qt5-only`（钉 `Qt${QT_VERSION_MAJOR}`）已撤销：审查指出
基座 `CMakeLists.txt:227` 自己就 `set(QT_VERSION_MAJOR 5)`，经复核属实，故本轮
零补丁。

> 备注：审查报告里"无需 `CMAKE_SKIP_INSTALL_RULES`"的结论是在"由 qlitehtml
> 自带分支引入 litehtml"的配置下测的（那一路带 `EXCLUDE_FROM_ALL`）；本设计是
> 自己直接引入 litehtml，install 规则照常注册，故该夹子保留，落地时按 §4.6 的
> 复核办法确认。
