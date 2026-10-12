# 界面双路线与 FFT 选型设计

状态：**已决策，实施中**（2026-10-12 定案；§六 第 1~4 步已完成并实测）。
审查记录见文末附录。

## 一、问题与目标

本仓的定位是**基础环境**：别人在它之上写业务逻辑，不需要关心三方库从哪来、
怎么编、怎么打包。界面部分是"备好两条路，使用者挑一条走"，FFT 部分则是
"配一个库就够，不留两套"。

**目标**

1. 把 `FluentUIStyle`（WinUI3/Fluent 风格的 `QProxyStyle` 实现）接入本仓，
   与现有"QSS 样式表 + SARibbon"这条路**并存**，两条路都能编、都能打包。
2. FFT 库收敛成一个：`fftw` 与 `kissfft` 二选一，另一个下线。
3. 上述两条都不破坏"别人不用管三方库、不用管打包构建"这条底线：新增的库
   走本仓既有的 3rdparty 流水线（tarball 入库 → configure 自动解压 →
   `add_subdirectory`），部署与打包自动覆盖。

**非目标**

- 不做运行期逐窗口混用两套样式（见 §4.1 的硬约束）。
- 不把 `FluentUI3StylePlugin`（Qt 样式插件）纳入分发——它要求把 DLL 写进
  Qt 安装目录，与"自包含"相悖。
- 不重写业务界面。两条路的样板以现有 `demo_app` 为准，不在本次改动里
  重做 UI。

## 二、现状回顾

**3rdparty 流水线**（已有 14 个包，见 `cmake/extract_dependencies.cmake:8`
的 `DEPENDENCY_ARCHIVES`，机制说明见 `docs/guide/第三方依赖补丁.md`）：
tarball 入库 → configure 时解压到 `3rdparty/<包名>/` → 必要时打补丁
（`cmake/patch_dependencies.cmake`）→ 顶层 `add_subdirectory`。

**部署与打包对新库不是自动的**（初稿这里写错了，审查时实测订正）：

- `make_deploy` 只是 windeployqt，它只管 **Qt 自己**的 DLL 与插件，不认识
  我们引入的第三方 DLL。
- 安装包的 `[Files]` 段是**白名单**（`cmake/template/make_install_package.iss.in:60-98`）：
  Qt 侧靠 `Qt5*.dll` 通配进去，但 `FluentUI3Style.dll` 这类必须**显式加一行**，
  否则装出来的程序缺 DLL 起不来。
- 构建产物落到 `build/bin/`（`CMakeLists.txt` 的 `CMAKE_RUNTIME_OUTPUT_DIRECTORY`）
  保证**本地直接能跑**；发布路径要单独接线（见 §六 第 1 步与 §4.4 D6）。

**样式现状**

| 项 | 现状 | 位置 |
|---|---|---|
| 项目层 QSS | 38 条 `#objectName` 规则，启动时 `setStyleSheet` | `src/app/resources/styles/default.qss`；加载处 `src/app/main.cpp:347-356` |
| 底座层 QSS | 已迁往 qt-libui 仓库，**本仓未接回** | 同上注释 `src/app/main.cpp:342-346` |
| 主窗口 | `SARibbonMainWindow`（`BUILD_RIBBON=ON` 时）或 `QMainWindow` | `src/app/mainwindow.h:10`；开关 `CMakeLists.txt:188` |
| 无边框 | QWindowKit 静态编、只编 Widgets、不 install | `CMakeLists.txt:518-535` |

**QWindowKit 的关键点**：`3rdparty/qwindowkit-1.5.0/src/CMakeLists.txt:88`
**无条件**创建了 `add_library(${QWINDOWKIT_INSTALL_NAME}::${_name} ALIAS ...)`，
而 `QWINDOWKIT_INSTALL_NAME` 在 `3rdparty/qwindowkit-1.5.0/CMakeLists.txt:99`
取的是项目名（`QWindowKit`），故 `QWindowKit::Widgets` 别名在本仓构建树里存在。
> 实施第一步要加一句 `if(NOT TARGET QWindowKit::Widgets)` 守卫断言这一点
> ——别名不进 `build.ninja`，只能靠源码结论 + configure 期断言。

**FFT 现状**：`option(BUILD_FFTW ... ON)`（`CMakeLists.txt:191-198`），接线在
`CMakeLists.txt:384-425`（静态、单精度、线程合并）。全仓唯一使用方是
`examples/fft_demo`（`fft_processor.cpp` 用 `fftwf_plan_dft_r2c_1d/c2r_1d`
+ 可选 `fftwf_plan_with_nthreads`）。FFTW 3.3.11 的许可证是 **GPL-2.0**
（`3rdparty/fftw-3.3.11.tar.gz` 内 `COPYING`）。

**本次已入库的两个包**

| 包 | 大小 | 内容 |
|---|---|---|
| `3rdparty/FluentUIStyle-4.0.tar.gz` | 12.7 MB | `FluentUI3Style`（8 cpp）+ `Frameless`（2 cpp）+ `ExWidgets`（31 cpp）+ 插件（1 cpp，不编）+ 示例；自带 `3rd/qwindowkit`（同版本 1.5.0）与 `3rd/kissfft`（裁剪版，7 文件） |
| `3rdparty/kissfft-131.2.0.tar.gz` | 52 KB | 上游完整包：库 + `test/` + `tools/` + `COPYING` + `LICENSES/{BSD-3-Clause,Unlicense}` |

## 三、依赖对账

新库自带的依赖与本仓已有的，逐项对账，**结论：都不需要重复编**。

| 依赖 | 新库要什么 | 本仓已有什么 | 结论 |
|---|---|---|---|
| QWindowKit | `frameless/CMakeLists.txt:7` 写的是 `if(NOT TARGET QWindowKit::Widgets)` → 才去编自己 `3rd/qwindowkit` | 同版本 1.5.0，静态，且别名存在（§二） | 用本仓那份，**不重复编**；前提是 `add_subdirectory(qwindowkit)` 排在 FluentUIStyle **之前** |
| kissfft | `ExWidgets/CMakeLists.txt:3` 写的是 `if(NOT TARGET kissfft)` → 才去编自己 `3rd/kissfft` | 我们先把上游 kissfft 定义成 target（名字就叫 `kissfft`，见 `CMakeLists.txt:123`） | ExWidgets 自动复用我们的，**不重复编** |
| Qt 组件 | Core/Gui/Widgets/Svg | 本仓已 `find_package(Svg)` | 满足 |
| 许可 | FluentUIStyle: MIT；kissfft: BSD-3-Clause 或 Unlicense（双许可，`LICENSES/`） | 本仓 `LICENSE` | 均兼容，无 GPL 传染 |

**一个残留细节**：`ExWidgets/CMakeLists.txt:40` 把 `../3rd/kissfft` 硬编码进
私有头文件搜索路径。即使 target 复用我们的，ExWidgets 的头仍从它自带的
裁剪版里找。两者同源（同一上游release 的裁剪），实测兼容；**若将来升级
上游 kissfft 造成头/库不一致，再对 FluentUIStyle 打补丁改这一行**——现在
不动它，避免无谓的补丁维护。

### 3.1 qmsetup：两份同源，且根本走不到一起

FluentUIStyle 的 `3rd/qwindowkit/qmsetup/` 与本仓 `3rdparty/qmsetup-4a3ff82/`
是**同一个上游项目**，版本一老一新：

| | 本仓（4a3ff82） | FluentUIStyle 自带（85c6c3c） |
|---|---|---|
| `project()` 版本 | **1.0.0.0** | 0.1.1.1 |
| 实质差异文件 | —— | `CMakeLists.txt`（版本号）、`cmake/QMSetupAPI.cmake`（两处：`qm_find_qt` 的 `foreach` 写法、`qm_include_qt_private` 缺 Linux 私有头回退、`qm_configure_target` 的 INTERFACE 库作用域处理） |
| 其余文件 | `src/CMakeLists.txt`、`CompilerOptions.cmake`、`LICENSE` 等逐字节一致（差异主要是行尾：本仓那份是 CRLF，他们那份 LF——对 CMake 脚本无影响） |

**两份的 `qm_*` API 对 QWindowKit 1.5.0 都够用**（证据：本仓用 4a3ff82 编过
QWindowKit；你在 `D:\software\aa-sources` 那份用 85c6c3c 也编过同一个
1.5.0）。他们那份的本地调整是给 Qt 5.12 的 MinGW GCC 7.3 用的，与本仓
（MSVC）无关。

**更重要的是：两份走不到同一个构建树里。** 本仓的 qmsetup 不是
`add_subdirectory` 进来的，而是由 QWindowKit 经 `qm_install_package` 在
configure 期编到独立目录、再 `find_package` 成**导入目标**
（`3rdparty/qwindowkit-1.5.0/CMakeLists.txt:108-131`，补丁改的只是
`_source_dir` 指向）。因此：

- **门禁一**：`frameless/CMakeLists.txt:7` 的 `if(NOT TARGET QWindowKit::Widgets)`
  —— 本仓的 `QWindowKit::Widgets` 是真的 `QWKWidgets` 目标的别名（非导入目标，
  全局可见）→ 它整个 `3rd/qwindowkit` 子树（含自带 qmsetup）**不会进入构建**。
- **门禁二**（万一门禁一失效）：他们的 qwindowkit 自己也有
  `if(NOT TARGET qmsetup::library)`（`3rd/qwindowkit/CMakeLists.txt:110`），
  而两份 qmsetup 的 `_corelib_name` 都是 `library`（各自
  `src/CMakeLists.txt:25-26`，逐字节一致）→ 也拦得住。
- **万一两层都失效**：各自的 `qm_install_package` 会往**各自的**临时目录里
  编一份、再各自 `find_package` 成**各自目录作用域**的导入目标——不会
  target 重名报错，代价只是白编一遍。

**顺序约束**（与 §三开头同一条链）：`qmsetup（经 qwindowkit 的 configure 期安装）→
qwindowkit → FluentUIStyle`。顶层 `add_subdirectory(3rdparty/FluentUIStyle-4.0)`
必须排在 `add_subdirectory(3rdparty/qwindowkit-1.5.0)` 之后。

### 3.2 ExWidgets 无条件编：kissfft 因此是硬依赖

初稿把"不编 ExWidgets"当成了一个可选项，**它不存在**：上游
`CMakeLists.txt:157` 的 `add_subdirectory(ExWidgets)` 不在任何 `if` 内
（上游只提供 `EXWIDGETS_BUILD_TESTS`，默认 OFF），`install(TARGETS ExWidgets ...)`
同样无条件；而 `ExWidgets/CMakeLists.txt:49` 又 `PRIVATE` 链 `kissfft`。

**推论**：接入 FluentUIStyle 就一定会编 kissfft——这与 D2 无关。真要"不编
ExWidgets"，只能打补丁，那就与"FluentUIStyle 无补丁"的说法冲突（见 D2）。


## 四、方案一：UI 双路线

> **决策已定（2026-10-12）**：D1=qss、D2=编但不用、D3=各自独立、D4=**qt-libui 留着**、
> D5=先不翻、D6=进包。实施按 §六 走。


### 4.1 三条硬约束（决定了方案形态）

1. **同一个进程里样式二选一**。只要 `setStyleSheet` 生效，命中规则的控件就
   走 `QStyleSheetStyle`，`QProxyStyle` 的绘制被绕过；反之亦然。两套叠加的
   结果是"一部分控件 Fluent、一部分 QSS"。
2. **同一个顶层窗口的 window agent 二选一**。SARibbon 经 QWindowKit 装自己
   的 agent，Fluent 的 `Frameless` 也对同一窗口 `setupWindowAgent()`，
   不能同时管。
3. 因此**选路必须在构建期或启动期定死**，不能做成"逐窗口混用"。

### 4.2 推荐形态

一个构建开关 + 一处启动分支 + 两条路各有一个能跑的样板：

```cmake
# 顶层：是否把 Fluent 那条路的库编进来并打包（默认 ON，别人不用管）
option(BUILD_FLUENT_UI "构建 FluentUI3Style 界面库（另一条界面路线）" ON)

# 顶层：demo_app 走哪条路（qss = 现有 QSS + SARibbon；fluent = FluentUI3Style + Frameless）
set(APP_UI_ROUTE "qss" CACHE STRING "demo_app 的界面路线：qss / fluent")
```

- **两条路的库都编、都打包**（`BUILD_FLUENT_UI=ON`）→ 满足"别人不用管三方库"
- `APP_UI_ROUTE` 决定 `demo_app` 链哪个、`main.cpp` 走哪个分支：
  - `qss`：现状不动（`setStyleSheet(default.qss)` + `MainWindowBase =
    SARibbonMainWindow/QMainWindow`）
  - `fluent`：`app.setStyle(new FluentUI3Style)`，**不加载任何 QSS**，
    `MainWindowBase` 换成 `QMainWindow` + `FluentWindowFrame`
- 两条路各有可复制样板：`qss` 就是现在的 `demo_app`；`fluent` 是同一个
  `demo_app` 换开关编出来的形态（业务代码不变，只换壳）——这最能说明
  "两条路只是壳不同"

### 4.3 备选与为什么不选

| 备选 | 结论 |
|---|---|
| 各给一个独立 example（`examples/fluent_demo`） | 可行，但样板会分叉成两份业务代码，长期要同步。**如果是"想同时看到两条路的对比"，可以作为 `demo_app` 之外的补充** |
| 运行期切换（`--fluent` 命令行） | 样式层面可行（启动时二选一即可），但窗口框架是**结构差异**（SARibbon 与 FluentTitleBar 不是同一套 chrome），要在窗口构造处再分一次支。收益（省一次重编）小于复杂度，**不推荐**；真要做，也是在 `APP_UI_ROUTE` 之外再加一层，不是替代 |

### 4.4 待决策点（本文件评审后逐条拍）

| # | 决策 | 选项 | 倾向 |
|---|---|---|---|
| D1 | `APP_UI_ROUTE` 默认值 | `qss` 保持现状 / `fluent` 立为新默认 | **qss**（不破坏现状，Fluent 用开关打开） |
| D2 | `ExWidgets` 要不要编 | 上游没有开关（见 §3.2）：A) 打一条补丁关掉；B) 编但不用、也不进安装包 | **B**——零补丁零维护；`ExWidgets.dll` 只躺在 `bin/`（本地构建产物），发布件不带它。kissfft 反正是要留的一等依赖 |
| D3 | SARibbon 与 Fluent 的关系 | 保持两条路各自独立 / 允许"Fluent 样式 + SARibbon 壳"混搭 | **各自独立**（混搭是 4.1 约束 2 的反面） |
| D4 | qt-libui 的定位 | 作为路线 B 的底座接回来 / 不再接（路线 B 只用项目层 QSS） | **待定**——取决于你还要不要那套自研底座 |
| D5 | 项目层 38 条 QSS 规则 | 走 fluent 时逐条翻成 Fluent 属性（`fluentui3styleproperties.h`）/ 先不翻，接受 fluent 路线下这些规则失效 | **先不翻**（把"翻规则"作为独立任务，`fluent` 路线先以"业务控件用 Fluent 默认观感"起步） |
| D6 | `FluentUI3Style.dll` 进不进安装包 | A) 进（`.iss` 补一行 + `skipifsourcedoesntexist`）B) 只在 `APP_UI_ROUTE=fluent` 时进（`.iss` 加 `#if`）C) 先不进，等真用 fluent 路线发布再说 | **A**——几百 KB 的死重量换取 .iss 零分支；`qss` 路线下它没被链接，装了也不会被加载 |

## 五、方案二：FFT 收敛

### 5.1 对比

| | FFTW 3.3.11（现在） | kissfft 131.2.0（已入库） |
|---|---|---|
| **许可** | **GPL-2.0** | **BSD-3-Clause 或 Unlicense**（双许可） |
| 构建代价 | 每次 configure 跑编译器与 SIMD 探测；重编耗时显著 | 5 个 .c，数秒编完 |
| 能力 | 大点数/批量变换的性能天花板 | 混合基实现，够用级别；单线程 |
| 与本仓关系 | 只有 `examples/fft_demo` 用它 | 只有 ExWidgets 频谱控件用它（若编 ExWidgets） |
| 产物名 | `fftw3f` | target `kissfft`、别名 `kissfft::kissfft`（`:238`），库文件 `kissfft-float` |

**GPL 是决定性差异**：基础环境给别人开发业务逻辑，链了 FFTW 就意味着业务
代码整体受 GPL 约束（或另行购买 FFTW 商业授权）。kissfft 没有这个问题。

### 5.2 推荐：留 kissfft，下线 fftw

- kissfft 提升为**一等三方依赖**（像 zlib/spdlog 那样自己配 `add_subdirectory`），
  而不是躲在 ExWidgets 里——"基础环境配一个 FFT 库"这句话的落点就在这里。
- `examples/fft_demo` 从 fftw 移植到 kissfft：`fftwf_plan_dft_r2c_1d/c2r_1d`
  → `kiss_fftr_alloc/kiss_fftr/kiss_fftri`，把 `FftwBuffer`/`FftwPlan` 两层
  RAII 换成 kissfft 的等价包装（kissfft 的缓冲用 `malloc`/`free`，无对齐要求）。
  `fftwf_plan_with_nthreads` 那处直接删（kissfft 单线程，demo 规模不需要）。
- 随后下线，**五处，一处都不能漏**（初稿漏了后三处，审查时补）：
  1. `BUILD_FFTW` 段：`CMakeLists.txt:191-198` 的 option 与 `384-425` 的接线
  2. `cmake/extract_dependencies.cmake` 里的 fftw 条目
  3. `3rdparty/fftw-3.3.11.tar.gz`
  4. `patches/fftw/` 的两条补丁**连同
     `cmake/patch_dependencies.cmake:131-142` 的注册行**——
     `apply_patch_to_dir` 在补丁文件缺失时是 `FATAL_ERROR`
     （同文件 `:32-34`）：只删文件不删注册，下次 configure 直接挂
  5. 文档引用：`docs/guide/第三方依赖补丁.md` 拿 FFTW 当范例讲了三处
     （option 与补丁的对比表、CMP0077 与抬版本那条）、`CLAUDE.md` 规则 18
     提到它、`docs/guide/模板使用指南.md` 也点过名——这些例子要么换人
     （SARibbon/QtTesting 同样是"补丁改上游"的样本），要么删

### 5.3 落地要点：四个 cache 变量与三条理由

cache 变量（在顶层 `add_subdirectory` **之前**设好）：

```cmake
set(KISSFFT_STATIC    ON  CACHE BOOL "" FORCE)  # 默认 OFF
set(KISSFFT_TEST      OFF CACHE BOOL "" FORCE)  # 默认 ON
set(KISSFFT_TOOLS     OFF CACHE BOOL "" FORCE)  # 默认 ON
set(KISSFFT_PKGCONFIG OFF CACHE BOOL "" FORCE)  # 默认 ON
# KISSFFT_DATATYPE 默认 float（上游 CMakeLists.txt:44），与 fft_demo、
# ExWidgets 的用法一致，不用改
```

三条理由（初稿写反了两条，审查时用最小工程实测订正）：

1. **`TOOLS`/`TEST` 必须 OFF，首因是"默认选项下 configure 直接失败"**：
   `tools/CMakeLists.txt:18` 的 `pkg_check_modules(libpng REQUIRED ...)` 与
   `test/CMakeLists.txt:32` 的 `pkg_check_modules(fftw3 REQUIRED ...)` 在缺
   pkg-config/libpng 时是 FATAL。"不往 ctest 塞用例"只是附带好处（与本仓
   `第三方依赖补丁.md` 记的 zlib 同类）。
2. **`STATIC` 要 ON，但理由不是"全局污染"**：`KISSFFT_STATIC=OFF` 时上游
   （`:88`）执行的 `set(BUILD_SHARED_LIBS ON)` 是**子目录作用域内的普通变量，
   不会外泄**（实测：add_subdirectory 之后父目录该变量仍为空、兄弟目录新建
   目标仍是 STATIC）。要 ON 的真实理由是**不让 kissfft 自己变成要部署、要
   打包的 `kissfft-float.dll`**。
3. **`PKGCONFIG` OFF** 只是少一条 `install(FILES *.pc ...)`，清噪音。

### 5.4 风险

- **性能上限**：大点数、批量变换 kissfft 明显慢于 FFTW。本仓的用途（演示、
  频谱）远在够用范围内；将来真需要极限性能，替换点集中在 `fft_demo` 与
  业务侧的调用处，可控。
- **回退成本**：fftw 的 tarball 与补丁在下线时一并删除，回退需从 git 历史
  取回。若想留后路，可只删默认值（`BUILD_FFTW=OFF`）而保留代码——但那是
  "留两套"，与本次目标冲突。**倾向：删干净**。

## 六、实施步骤（每步独立可验证，默认路线不受影响）

> **进度（2026-10-12）**：第 1~4 步已完成；第 5 步（下线 fftw）已随第 2 步一并
> 做完；第 6 步（文档与规则同步）进行中。
>
> 两条路线的构建/运行命令（实测过）：
>
> ```bash
> # qss（默认）
> scripts\build_ninja.bat -t relwithdebinfo -a x86
> # fluent —— 独立构建目录，不去动默认目录缓存里的 APP_UI_ROUTE
> cmake -S . --preset ninja-relwithdebinfo-x86 \
>       -B build/ninja-relwithdebinfo-x86-fluent -DAPP_UI_ROUTE=fluent
> cmake --build build/ninja-relwithdebinfo-x86-fluent
> ```
>
> 实测：fluent 版的 exe 只多依赖 `FluentUI3Style.dll`（没有 ExWidgets.dll、
> 没有 QWK DLL——QWK 是静态链进去的）；两个版本都能起来，日志里各自打印
> 「界面路线：…」那一行（`%LOCALAPPDATA%\Enpht\<应用名>\logs\`）。

1. **入库两个包**：解压清单加两行；FluentUIStyle 以 `BUILD_PLUGIN=OFF`
   （顺带关掉 `FLUENTUI3STYLE_COPY_TO_QT_DIR` 那条拷贝路径）、
   `BUILD_EXAMPLES=OFF`、`BUILD_QTCREATOR_PLUGIN=OFF`、`BUILD_LIBRARY=ON`、
   `BUILD_FRAMELESS=ON` 接入；kissfft 按 §5.3 接入。
   **同时要动一处旧接线**：QWindowKit 的 `add_subdirectory` 现在被
   `if(BUILD_RIBBON)` 包着（`CMakeLists.txt:517-536`），要提到外面来
   （`BUILD_RIBBON OR BUILD_FLUENT_UI` 时编），否则 `-DBUILD_RIBBON=OFF`
   的构建里 `QWindowKit::Widgets` 不存在，守卫断言会 FATAL，而 FluentUIStyle
   会转而编它自带那份 QWK——同一个仓两套 QWK 栈，正是要避免的。
   守卫写成 `if(BUILD_RIBBON OR BUILD_FLUENT_UI)` 里的断言。
   → 验证：configure/build 通过，产物落 `bin/`、`lib/`，ctest 数量不变。
2. **kissfft 提升 + fft_demo 移植 + 安装包接线** → 验证：fft_demo 功能对齐
   （频谱显示与改前一致），构建时间对比记录；`.iss` 的 `[Files]` 段补
   `FluentUI3Style.dll`（若 D6 选"进包"）。
3. **接 FluentUI3Style 到 app**（`APP_UI_ROUTE=fluent`，不加载 QSS）→
   需要**新增一个宏**把路线传进业务代码：现有的 `APP_ENABLE_RIBBON` 只管
   SARibbon（`src/app/CMakeLists.txt:131-134`，`mainwindow.h:8-14` 据它选
   基类），Fluent 侧要加 `APP_UI_FLUENT`（`APP_UI_ROUTE=fluent` 时注入），
   同时管基类选择与"不链 SARibbonBar"。
   → 验证：两条路线各编一次，fluent 路线能跑起来。
4. **接 Frameless**（标题栏 + `installChromeHeader`）→ 此时落地 D3。
5. **下线 fftw**（§5.2 的四处）→ 验证：全量重编 + ctest + 打包。
6. **文档与规则同步**（§八）。

每一步做完都可以停：1、2 步与界面无关，3 起才动 app。

## 七、验证计划

- **构建**：两条路线 × `relwithdebinfo` x86（本仓口径）各一次；x64 至少一次；
  **WSL2 至少一次**——`ExWidgets` 是 SHARED 且私有链 `kissfft`，而 kissfft
  上游全文没有 `POSITION_INDEPENDENT_CODE`（本仓也没全局设），GCC 下把非 PIC
  的静态库链进 .so 通常在链接期报 `relocation R_X86_64_32S ...`。若命中，
  本仓侧一行 `set_target_properties(kissfft PROPERTIES POSITION_INDEPENDENT_CODE ON)`
  即可（属"上游该有却没有"，照 `第三方依赖补丁.md` 的判据登记）。
- **测试**：`ctest` 11/11（新增库不得往 ctest 塞东西，见 §5.3 理由一）。
- **部署**：`-t relwithdebinfo -a x86 -d` 后 `bin\` 里有 `FluentUI3Style.dll`
  （要 ExWidgets 时再加 `ExWidgets.dll`，debug 构建带 `d` 后缀）；
  **不需要 QWKCore/QWKWidgets DLL**——上游 `USAGE.md` 说"运行时需要 QWindowKit
  的 DLL"，那是按它自己的动态构建说的；本仓 QWK 是静态（`CMakeLists.txt:528`），
  `Frameless` 也是静态库，两个静态目标链在一起，不产生 DLL 边界。funq 工具
  目录不受影响（`make_deploy → make_funq_kit` 依赖链未变）。
- **打包**：`-p` 出安装包并安装验证。两个去处要分清：**安装包只收 exe 与
  DLL**（`.iss` 里没有任何 pdb 行），**PDB 只进 `dist/symbols/`**
  （`make_symbols` 现在只归档 exe + crashpad_handler，`cmake/archive_symbols.cmake`
  是通用的，可直接收新增 DLL）。
- **眼验**：fluent 路线截图（可复用 `temp/` 下的 funq 截图脚本），
  与 `-style FluentUI3` 的预览效果对照。

## 八、文档与规则同步

| 文件 | 改什么 |
|---|---|
| `CLAUDE.md` | QSS 分层规则补一句："走 fluent 路线时不加载任何 QSS，业务控件样式改用 `fluentui3styleproperties.h` 的属性"；常用命令段补两条路线 |
| `README.md` | 依赖清单（现在只列了 4 个，本就滞后）；两条界面路线各一段 |
| `docs/guide/模板使用指南.md` | 新增"选界面路线"小节 |
| `docs/guide/发布指南.md` | 符号归档范围加新增 DLL 的 PDB |
| `CHANGELOG.md` | 新增/移除各一条 |
| `docs/guide/第三方依赖补丁.md` | 两个新包的说明（都无补丁）；若下线 fftw，拿它当范例的三处要换人或删（见 §5.2 第 5 条） |
| 许可随包 | `FluentUI3Style` 是 MIT，MIT 要求"所有副本中保留版权声明与许可文本"，而它会被打进安装包分发出去。本仓有先例：funq 的许可证与源码是整目录随包的（`src/app/CMakeLists.txt:362-366`、`.iss:89-94`）。照做：把 `LICENSE` 与 `THIRD_PARTY_NOTICES.md` 随包带一份（或加"第三方组件与许可"一节） |

## 九、风险与未决问题

1. **最大成本项是"翻 QSS"**（D5）：fluent 路线下 38 条项目层规则失效，
   登录框等待重做。这是选 Fluent 路线的真实代价，不是接线成本。
2. **上游迭代**：FluentUIStyle 4.0 是活跃项目，升级 = 换 tarball + 重跑
   §六 的 1、3、4 步。
3. **Qt 目录污染（只涉及 x64 那套 Qt）**：插件默认
   `FLUENTUI3STYLE_COPY_TO_QT_DIR=ON`（`fluentui3style/plugin/CMakeLists.txt:78`）
   会把插件写进 Qt 安装目录。我们 `BUILD_PLUGIN=OFF` 不受影响；但本机
   **x64** 的 `msvc2019_64/plugins/styles/` 里已经有一份（你在
   `D:\software\aa-sources` 那次 Debug 构建拷进去的
   `FluentUI3StylePlugind.dll` + `.pdb`）——留着就能"零改动预览"
   （`demo_app.exe -style FluentUI3`）。注意这条预览路**只有 x64 走得通**：
   本仓验证口径的 x86 Qt（`msvc2019/plugins/styles/`）里没有插件，且
   release 构建也不会去认带 `d` 后缀的插件（Qt 只在 debug 构建里找 `d`）。
4. **动态库与调试后缀**：`FluentUI3Style`、`ExWidgets` 是 SHARED，debug 带
   `d` 后缀（`FluentUI3Styled.dll`），发布走 relwithdebinfo 无后缀。
   构建侧产物落 `bin/` 不用管；**发布侧两件事要接**：安装包 `[Files]` 补行
   （D6）、符号归档扩到新增 DLL 的 PDB。
5. **上游的 `install()` 规则会真的执行**：FluentUIStyle 根 CMakeLists
   （`CMakeLists.txt:180-300`）有一整套 `install(TARGETS ...)` + 头文件目录 +
   `install(EXPORT ...)`。Windows 侧只跑 ISCC 不受影响；**Linux 侧
   `make_appimage` 走的是 `cmake --install`**（`cmake/template/make_appimage.sh.in:119`）
   → AppDir 里会多出一堆头文件与 cmake 包配置。另外它开头
   （`:74-86`）在 `CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT` 时会把
   prefix **FORCE 成 Qt 安装目录**——本仓两条正规路径都显式设了 prefix
   （preset 与 AppImage 的 `--prefix`）所以碰不到，但"不带 preset 直接
   `cmake -B`"的人会看到缓存被改。要在 Linux 打包上做干净，只能给它打一条
   关掉 install 的补丁；Windows 侧可以先不管。
6. **未决**：D1–D6（§4.4）。其中 D4（qt-libui）需要你给个方向——
   如果 Fluent 路线成为主线，那套自研底座是"路线 B 的底座"还是退役？

## 附录：审查记录

2026-10-12 第一轮（subagent，逐条核对引用 + 解包核实 + 用本仓自带 cmake 做
最小工程实验；实验工程留在 `temp/review_fuis_exp*`）。结论：整体可行，
§三 的两条 CMake 机制假设被独立证实（另有一条更硬的旁证：本仓 x86 构建树里
`SARIBBON_USE_3RDPARTY_FRAMELESSHELPER` 出现 63 次，该宏只在
`if(TARGET QWindowKit::Widgets)` 为真时才定义）。发现并**已订正**：

| # | 级别 | 问题 | 订正 |
|---|---|---|---|
| 1 | 🔴 | "部署与打包已覆盖 bin\*"——打包这半是错的：`.iss` 的 `[Files]` 是白名单，第三方 DLL 不会进包 | §二 改写；新增 D6 与 §六 的 .iss 步骤 |
| 2 | 🔴 | §5.3 两条"陷阱"方向都写反了：`BUILD_SHARED_LIBS` 不外泄（实验证伪）；而 TOOLS/TEST 默认 ON 的真实后果是**configure FATAL**（`pkg_check_modules(... REQUIRED)`） | §5.3 重写为三条理由 |
| 3 | 🔴 | "不编 ExWidgets"这个选项不存在（上游无条件 `add_subdirectory(ExWidgets)`），kissfft 因此是硬依赖 | 新增 §3.2；D2 改为"补丁关掉 / 编但不用" |
| 4 | 🟡 | QWindowKit 被 `if(BUILD_RIBBON)` 包着，`-DBUILD_RIBBON=OFF` 时守卫断言会 FATAL | §六 第 1 步写明要把它提到 `BUILD_RIBBON OR BUILD_FLUENT_UI` |
| 5 | 🟡 | 基类切换需要新宏（现有只有 `APP_ENABLE_RIBBON`） | §六 第 3 步写明新增 `APP_UI_FLUENT` |
| 6 | 🟡 | Linux 侧 kissfft 缺 PIC 可能链接失败 | §七 补 WSL2 验证与一行修法 |
| 7 | 🟡 | 上游 `install()` 规则会随 `cmake --install` 执行；其开头还会 FORCE 改 `CMAKE_INSTALL_PREFIX` | §九 新增第 5 条 |
| 8 | 🟡 | 下线 fftw 的清单漏了三处（补丁注册会 FATAL、文档里三处以它为例） | §5.2 扩成五处 |
| 9 | 🟡 | MIT 要求许可随副本；本仓 funq 已有随包先例 | §八 新增"许可随包"行 |
| 10 | 🔵 | 引用与计数小错（`QWINDOWKIT_INSTALL_NAME` 的定义位置、cpp/文件计数、插件预览只对 x64 成立、PDB 去处） | 已逐条订正 |

待决策：D1–D6（§4.4）。
