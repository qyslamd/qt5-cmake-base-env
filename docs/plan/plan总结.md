# plan 文档归档总结

`docs/plan/` 只保留这一份与尚未收尾的方案：已收尾的方案提炼到这里后删原文，
原文用 `git log --diff-filter=D -- docs/plan/<文件名>` 查；未处理的问题作为
活清单放在最后一节。新写方案时在 `docs/plan/` 下另起文件，收尾后再并进来。
仍在进行/暂缓的方案（图片懒加载、重排去抖步 3、Mermaid 线程化等）继续以
独立文件保留。

## 单实例 + 崩溃捕获接入，与 Linux 构建支持（2026-09-11，原同名文档）

**仍有效**

- D2 单实例键取 `applicationName()`，对占位值 `"FILE"` 防呆告警
- D4 Linux 边界：configure + build + test（+ AppImage），不移植
  windeployqt / Inno Setup
- D6 致命信号归 CrashHandler，GEH 只管 Qt 消息重定向 + SIGINT/SIGTERM，
  **不是**双层防护 → `main.cpp:282`

**坑**

- include 大小写：Windows 不敏感，Linux 直接编译失败
- `#ifdef _DEBUG` 是 MSVC 专属宏，GCC 下 Debug 不定义；判断调试构建用 `NDEBUG`
- 非 Windows 没有 HWND 激活那条路，靠 `showApplication` 信号手动显示窗口

**已作废**：§3.5 给 Linux 的命令是 `cmake --preset ...`（Linux 侧不走 preset，
`binaryDir` 与 Windows 侧同名会互相覆盖缓存），现用 `scripts/build_ninja.sh`。
§6 遗留项了结三条，只剩 `single_instance.cpp` 的 `_WIN32` 守卫风格（MSVC-only，不改）。

## tour 模块的四处缺陷（2026-09-22，原《引入的tour的bug记录》）

- B1 / B2 模式状态存了两份（控制器 + 演示层），跨主题只更新一份；且
  `auto_mode_` 全仓没有置回 true 的路径 → 收口成一个 `syncPresentationMode()`
- B3 `time_percent_` 的语义是"已过去的比例"，初值给反成了 1.0
- B4 **被显式 `hide()` 过的子控件，不会随父控件 `show()` 回来**
  （`showChildren()` 跳过 `isHidden()` 的子控件）→ `tour_overlay.cpp:387`

B1 / B2 只在多主题、且其中一条是实操式时显形；`tour_demo` 三条主题跑一遍
「全部演示」即覆盖。未做完的两条见文末挂起清单第 5、6 条。

### 又五处（2026-10-03，逐文件复审后修）

- **B5 实操模式的鼠标穿透从来没生效过**。`ApplyTopicOptions` 里连着调
  `setInteractive(topic->interactive())` 再 `setPassiveMode(false)`，两个 setter
  各写一次 `WA_TransparentForMouseEvents`，后者无条件把前者刚设的 true 盖回
  false。后果：hands-on 主题里被高亮的控件点不动，文案承诺的"点遮罩空白不退出"
  也反向——点空白反而 `emit skipClicked()` 退出。**B1/B2 的同一形态下沉了一层**：
  状态从"两份"变成了"一处状态两个写者"。修法：属性由
  `passive_mode_ || interactive_` 现算，只留 `UpdateMouseTransparency()` 一个出口
- **B6 卡片样式有两个出处**。`TourCard::paintEvent` 无条件自绘底色与边框，
  且画在 QSS 之后——`tour_default.qss` 里的 `#tourCard` 实际被盖住，
  改 QSS 没反应；而头文件注释写着"QSS 覆盖、无 QSS 时自绘兜底"，不成立。
  修法：自绘整段删除，hover 走 `:hover`，选中/占位走动态属性
  `selected` / `placeholder`（改属性后 `unpolish` + `polish`）
- **B7 首页标题写死了产品名**。`tour_homepage.cpp` 里写死"欢迎使用
  EP-X6502 PNP演示程序"——这个字串全仓只此一处，对不上任何东西（是本模块从
  老 Demo 抽出来时带过来的）。引导是通用库，改成默认取宿主的
  `applicationName()`，并加 `setHeading()` 供宿主覆盖
- **B8 "最后一条"有两处口径**。`refreshStepOverlay` 拿
  `topics().count() - 1` 判末尾，而 `nextPlayableTopicIndex` 跳的是**最后一个可
  播放**的主题（首页会给 `stepCount()==0` 的占位主题建卡）。末尾是占位主题时
  最后一步永远显示"下一步"而非"完成！"；首端是占位主题时"上一步"亮着却点不动
  （`previousPlayableTopicIndex` 返回 -1 直接 return）。修法：两处都改用这两个
  谓词，"还有没有下一站"只留一个出处
- **B9 菜单文字与实际播的内容脱钩**。`TourMenu` 的「模板导览」播的是
  `topics().first()`，`main.cpp` 的 `guideRequested` 槽同样——靠注册顺序维系。
  多注册一条主题就播错。修法：主题由宿主显式传入 `attachMenu(window, topic)`

B5–B9 都**没有用例覆盖**（`tests/` 下没有 tour 目标）。编译与
`ctest -E test_funq_app` 21/21 通过只能证明没编坏，证明不了这五处的运行时行为；
B5 与 B6 需要人眼/手验一次。

### 再三处（2026-10-04，同一模块的静态缺陷）

- **B10 `TourOverlay` 析构漏气泡**。`bubble_widget_(new QWidget(this))`
  本来挂在遮罩下，但实操模式的 `setInteractive(true)` 里有
  `bubble_widget_->setParent(nullptr)`——把它提成了独立顶层窗口，**同时摘掉了
  Qt 的父子所有权**。类里没有析构函数，于是"进过实操模式再销毁"就漏一个
  QWidget，而且那个窗口会一直留在屏幕上。修法：加析构显式 `delete`，
  仍挂在遮罩下时删它同样安全
- **B11 `EnsureApplied` 提前置位导致样式永不生效**。`applied = true` 写在两个
  early return **之前**：`app == nullptr`（QApplication 还没建）或 QSS 读取失败
  时，标记已经置真，之后每次调用都在第一行返回——样式永远不生效，且不报错。
  修法：只有真的 `setStyleSheet` 成功之后才置真；前两条路各自留一次重试的机会
- **B12 getter 按值返回**。`TourStep` / `TourTopic` 的 10 个 getter
  全按值返回，其中 `Target` 是 variant（可能装 `QPixmap`）、`Func` 是
  `std::function`（按值拷可能堆分配）。而控制器里的写法普遍是
  `if (x->enterFunc()) { x->enterFunc()(); }`——**一次调用就拷两遍**；
  `updateUi` 里 `step->target()` 同样取了两遍。修法：全部改 `const&`，
  并把 `updateUi` 的局部量改成引用，去掉那次多余的拷贝

## libui 抽离为独立 CMake 库（2026-09-14，原同名文档）

抽出 `src/libui`（40 文件 / 3196 行）连同 gallery 成独立仓 `qt-libui`。
**接入方式后反转**：§9 判定"已评估未采用"的 submodule 最终被采纳（源码内嵌，
只取 `qt-libui::libui` 目标、不登记 install 规则）；**再后被整包移除**——
本仓现在不含任何 submodule，仅 `SwitchButton` 源码内联进 `src/app/widgets/`。

**坑**

- Debug（`/MDd`）链 RelWithDebInfo 静态库（`/MD`）是 `LNK1319 FAILIFMISMATCH`：
  **fatal，不是设计时判断的"可接受警告"**
- 装出去的头文件必须带 UTF-8 BOM，否则消费方不开 `/utf-8` 会按 936 代码页
  解析、报 `C2059`（本仓全局 `/utf-8`，不受影响）
- `GNUInstallDirs` 要早于 `add_subdirectory(src)`；`INSTALL_INTERFACE` 与
  `install(... INCLUDES DESTINATION)` 写两遍会让包含目录重复；头文件装到
  `include/qt-libui/` 时搜索根取上一级
- 库内源文件只能"同目录裸名"包含，写成从 src 根出发的路径：库编得过、
  消费方必炸

**已作废**：QSS 与 libui 的 objectName 跨仓隐式契约、libui 冻结——契约随库
一起消失（见文末"已决定不做"）。

## 通用样式表分层（2026-09-23，原《通用样式表设计》）

一份 704 行、135 个规则块的表里，滚动条/下拉框/微调框/分割器四个控件吃掉 65 块，
而 25 个常见控件连裸类型规则都没有；43% 的规则块挂在 `#objectName` 上，对别的
项目是死规则。改成**底座层 + 项目层**两层，底座层独立成模块。

**仍有效**

- **三档定位**（优先级从低到高）：类型 + 伪状态 + 子控件 → 动态属性
  （`setProperty("role", "primary")` 配 `[role="primary"]`；改完属性要
  `unpolish` + `polish`，否则不刷新）→ `#objectName`（仅项目层，且仅在无法用
  属性表达时）。**objectName 是 funq 与 QtTesting 的锚点，不拿来当样式角色**
- **两层顺序**：底座 → 项目层；**层内从宽到窄**——QSS 里同具体度的类型选择器
  "后写优先"，所以最宽的 `widget.qss` 必须置首，否则会盖掉 `input.qss` 之类
- 底座层做成模块（`src/style/`，目标 `demo_style`）而非散在 app 资源里，是为了
  "拿走即用"名副其实：链上模块调一次 `ApplyStyleSheet` 就有，文件清单只在
  `style.cpp` 一处
- 全局字体走 `QApplication::setFont`，不用 `* { font-family }`；拼接多份表要
  显式补 `\n`（末尾无换行会出现 `}QPushButton{` 粘连）
- **全局表的裸类型规则会打进功能区**：`setRibbonTheme()` 最终是
  `w->setStyleSheet(...)`——打在**主窗口**上的 widget 级表，而主题里只有
  `SARibbon*` 选择器；SARibbon 的控件全是标准件子类（`SARibbonBar : QMenuBar`、
  `SARibbonToolButton : QToolButton`、`SARibbonPanel : QFrame`…），不冲突的属性
  仍由 app 级表提供。**新增 `QMenuBar` / `QToolBar` / `QToolButton` / `QLabel` /
  `QListView` / `QAbstractButton` / `QScrollArea` 的规则时必须回看功能区**
- 自定义 `QWidget` 子类设 `background-color` 无效：要
  `setAttribute(Qt::WA_StyledBackground, true)` 或重画 `PE_Widget`。最容易让人
  以为"QSS 没生效"的一条
- 原生对话框（`QFileDialog`）与非客户区（窗口边框、标题栏）不在 QSS 能管的范围

**偏离方案的两处**：① tour 那份表**仍**追加到应用级（原计划改打自己子树）
——实操模式下气泡会被 reparent 到 viewport、离开首页子树，只设在首页盖不到；
真要切断这层顺序依赖，做法是设到顶层窗口上，需引导模块拿到宿主窗口。
② `QFrame` 与 `QSplitterHandle` 故意不给裸规则：`SARibbonPanel` /
`SARibbonGallery` 都是 QFrame 子类，一条裸 `QFrame` 会直接改功能区；
QSplitterHandle 只能通过 `QSplitter::handle` 表达。

**未做人眼确认**：功能区（画廊、滚动条）、登录框勾选框、主按钮是最可能露馅的
三处。底座层靠 `examples/style_demo`（按控件族分六页）眼验。

> **2026-10-10 后记**：底座层模块（`src/style/`，`demo_style`）与
> `examples/style_demo` 已整体迁往 qt-libui 仓库（`ApplyStyleSheet` 与 base 层
> QSS 随 `qt-libui::libui` 交付）；本仓 app 改为就地读项目层样式表。上文按
> 迁移前的结构记述，仅作历史参考。

> **2026-10-12 后记（第二次）**：`src/style/` 又搬回来了——**只搬底座样式表**，
> 与 qt-libui 不留任何依赖关系（那边已长成独立库，含自绘控件与三套 QProxyStyle，
> 本仓不需要）。做法：QSS 与两张箭头图按原样拷回（实测与 qt-libui 现版逐字节
> 一致），`ApplyStyleSheet` 与 app 的接线一并恢复。两仓从此**不再同步**，
> 上游有修复得手工比对。决策与理由见
> 本文档「界面双路线与 FFT 选型」的 D4。

## tour 接入 funq（2026-09-22，原同名文档）

把引导的演示层当 funq 的**可视化层**：脚本每动一个控件，屏幕上就有高亮框指到它。
平时给用户当新手引导，跑脚本时给自动化当"看得见的光标"。

**仍有效**

- **矩形在 C++ 侧按 path 解析，不依赖 funq**：bridge 收路径字符串，用引擎自己
  那套 `mapToGlobal` 算矩形——零依赖（tour 库保持干净）、与引擎同源（不会
  出现"算出来的框和引擎画的对不上"）。解析规则照 `libFunq/objectpath.cpp`
  **重写**：名字取 objectName、为空退类名、同名兄弟带 `-<序号>` 后缀、按 `::`
  下钻，所以含**合成段**的路径（`...settingsRow-1::QSpinBox`）也能解析。
  **不抄源码**：funq 是 CeCILL v2.1（copyleft），拷进来会把 tour 卷进那个
  许可证；算法本身不受版权保护
- 桥接的三条硬约束，都是实测撞出来的：**必须是可见的 QWidget**（`call_slot` 走
  `qobject_cast<QWidget*>`，纯 `QObject` 判 `NotAWidget`，且 `client.widget()`
  默认还要等 `visible`）；**必须是独立顶层窗口**（脚本按 `tourBridge` 单段路径
  寻址它，挂窗口下路径就多一段；`Qt::Tool` 不进任务栏）；**签名必须是"单个
  QVariant 进、QVariant 出"**（funq 固定按那种 `invokeMethod` 调，多形参直接
  `NoMethodInvoked`）
  - **2026-10-13 修订**：原先还有一条"必须早于登录就存在"（脚本第一步跑在登录
    框上）。引导已改到登录之后装配（main 里的 `SetupTour`），该约束作废；登录
    框不再做高亮
- **别在 Python 侧自己凑全局坐标**：funq 的 QtJson 序列化拿不到 `geometry` /
  `pos` / `size`（直接跳过），只给 int 的 `x` / `y` / `width` / `height`，且
  `x`/`y` 是**父相对**的
- 目标是 `QWidget`（引擎的目标类型只有控件 / 矩形 / 图），`QAction` 指不了——
  需要时改指它所在的菜单
- 目标不可见或尺寸为空时**拒绝高亮并回报原因**：踩过——菜单栏里的 `QMenu` 从没
  显示过，几何是未初始化的 `(0,0,100x30)`，指它会把遮罩铺到屏幕左上角一个小方块

**结构**：拆成两个类——`TourBridge`（**QWidget**：被动模式 + 按 path 高亮，
两个 `Q_INVOKABLE` 槽）与 `TourMenu`（`QObject`：给 QMainWindow 宿主挂菜单、
懒建主题主页）。原先合成一个 `TourSupport`，结果是菜单那半白白背着 QWidget
基类和"0 尺寸 + 必须 `show()`"的包袱——**它之所以是 QWidget 纯粹是 funq 对桥接
的要求**。被动模式加在 `TourOverlay`（渲染层）而非 controller。

## 崩溃报告回传链路（2026-09-28，原《崩溃报告回传链路设计》）

程序发给**离线客户**，崩溃后 dump 躺在 `%LOCALAPPDATA%` / `~/.local/share` 里
没人翻——"捕获"与"归档"都就绪，断在**回传**。崩溃那一刻进程正在死，没有弹窗
机会，所以提示只能放**下次启动**。

**能力边界（源码实证，别再重复讨论）**

- crashpad 固定写 `MiniDumpNormal`（`minidump_file_writer.cc:51`），**没有**
  DataSegs / FullMemory → 全局变量与堆对象内容不在 dump 里；extra memory ranges
  仅 Windows 有效；indirectly-referenced memory 的实现在 `snapshot/win/`
- handler 注解确实写进 dump，但落在 CrashpadInfo 扩展流里，**cdb / WinDbg 看不见**
  → 人可读副本走 `summary.txt`，dump 里那份只作留档
- 口径：只保证"栈 + 寄存器 + 模块 + 注解"。**RelWithDebInfo 下被优化掉的局部
  变量看不到是预期内的**

**决策**

- D1 导出成**目录**不压缩：Qt5 没有公开的 zip 写入口，为压缩引依赖不值当
- D2 提示放**登录前**（崩溃可能发生在登录前，且此时还没有主窗口）；回放模式与
  无桌面（`QT_QPA_PLATFORM=offscreen`）跳过
- D3 ~~`--crash` 由 `APP_ENABLE_CRASH_TEST` 控制~~ ——**2026-10-13 修订**：该宏与
  `--crash` 参数一并下线，演练改为主窗口「工具」里的"触发测试崩溃"动作；
  崩溃后端本身也改成运行时配置（`crash/backend`，重启生效）
- D4 自研模式本期不导出，只保证调用不崩
- D5 导出**不删除**报告：`SkipReportUpload(uuid, kUploadsDisabled)` 转入
  `completed/`——不再提示，又不会因为客户弄丢导出文件而永久失去唯一副本
- D6 报告标识用 **UUID 不用下标**：`GetPendingReports()` 每次返回新快照，按下标
  实现时"第一份被转移后原本的第 i+1 项顶到 i 位"会被跳过

**坑**

- **"取值可选"的选项在 Qt 里做不到**：`QCommandLineParser` 只要给了 valueName，
  即便默认值传空串，`process()` 仍报 `Missing value after '--export-report'.` 并
  结束进程 → 拆成 `--export-report` + `--export-dir <目录>`
- **导出入口不能被单实例逻辑吞掉**：`HandleAlreadyRunning` 只对 `--play` 特判，
  其余一律"转发给首实例后 return 0"，而转发对端只做窗口激活、不落地任何动作
  → 脚本会看到**假成功**。`--export-report` 与 `--play` 同等对待
- Windows 是 WIN32 子系统程序，交互式 cmd 不等它、也读不到退出码 → 由
  `export_crash_report.bat`（内部 `start /wait`）兜住，放 bin/ 随安装包发到客户机
- 无桌面时要用 `QT_QPA_PLATFORM=offscreen`：`QApplication` 先于参数解析构造，
  没有 DISPLAY 时进程在构造阶段就死
- Windows 上取库会**短暂独占锁** → 只在启动早期调用一次，不进循环 / 定时器；
  失败即视为 0 份继续启动
- 核心 API 一律做成**参数给全的纯函数**（`db_dir` / `target_dir` / `AppInfo` 由
  调用方传）：单元测试才能指向临时目录、不碰开发者本机的真实崩溃库。测试用
  `PrepareNewCrashReport()` + `FinishedWritingCrashReport()` 造假报告，
  **不需要真崩溃、不需要 GUI**
- `CrashReportDatabase::Initialize` 失败要当成"0 份"而不是错误——首次运行没有库
  是正常状态

**验收口径（唯一的硬验收）**：开发者拿到回传的 dump 能恢复出"函数名 + 文件:行"。
Windows 用 `cdb -z <dmp> -y <归档符号目录>`；Linux 用 lldb 直读 minidump——
**选项必须写在可执行文件之前**（`lldb -b -o "bt" -c <dmp> ./exe < /dev/null`），
写后面会挂住（gdb 9.2 不支持 minidump，别用它）。

## 界面双路线与 FFT 选型（2026-10-12，原同名文档）

一次办两件事：**接 FluentUIStyle 当第二条界面路线**（与"QSS + SARibbon"并存），
**FFT 收敛成一个**（kissfft 留、FFTW 下线）。底线不变：别人不用管三方库从哪来、
怎么编、怎么打包。

**三条硬约束（决定方案形态）**

1. 同一进程里样式二选一：`setStyleSheet` 一生效，命中控件就走 `QStyleSheetStyle`，
   `QProxyStyle` 的绘制被整个绕过（反之亦然），叠加的结果是一半 Fluent 一半 QSS
2. 同一顶层窗口的 window agent 二选一：SARibbon 与 Fluent `Frameless` 都要
   `setupWindowAgent()`
3. 因此选路必须在构建/启动期定死，不能逐窗口混用

**落地形态**：`BUILD_FLUENT_UI`（编不编那条路）+ `APP_UI_ROUTE`（demo_app 走哪条）
+ 宏 `APP_UI_FLUENT`（管基类与链库）。两条路都编、都打包。~~fluent 用独立构建目录
（`-DAPP_UI_ROUTE=fluent`），不动默认目录的缓存~~ 2026-10-14 更新：路线开关
下沉为普通变量（顶层默认 qss，`local.cmake`/子目录覆盖，单构建目录可混编；
`-D` 已失效），宏归一只留 `APP_UI_FLUENT`，`FUNQ_RIBBON` 判据随 demo_app 实际
路线——见本文「界面路线开关下沉到目标」一节。

**依赖对账**：新库自带的 qwindowkit / kissfft **一份都不用重复编**——靠上游的
`if(NOT TARGET ...)` 门禁复用本仓那份。两条顺序约束都是硬的：
`qmsetup → qwindowkit → FluentUIStyle`；**QWindowKit 必须排在 SARibbon 之前**
（SARibbon 配置期查 `TARGET QWindowKit::Widgets` 决定编不编 frameless，排反了
qss 路线的主窗口静默失去圆角与标题栏，只剩一行 WARNING）。`ExWidgets` 上游
无条件编且私有链 kissfft → **接入 FluentUIStyle 就一定会编 kissfft**，
"不编"这个选项不存在。

**FFT**：GPL 是决定性差异（FFTW 是 GPL-2.0，会把业务代码一起拽进去；kissfft 是
BSD-3/Unlicense 双许可）。kissfft 提升为一等依赖，`fft_demo` 移植过去。
下线 FFTW **五处一处不能漏**——尤其 `patches/fftw/` 的两条补丁与
`patch_dependencies.cmake` 的注册行必须同删（`apply_patch_to_dir` 在补丁文件缺失
时是 FATAL，只删文件不删注册，下次 configure 直接挂）。
kissfft 四个 cache 变量：`STATIC=ON`（否则编出要部署、要打包的
`kissfft-float.dll`）、`TEST/TOOLS=OFF`（默认 ON，而它们
`pkg_check_modules(... REQUIRED ...)`，缺 pkg-config 时 configure 直接 FATAL）、
`PKGCONFIG=OFF`（清噪音）。

**决策**

| # | 决策 | 定案 |
|---|---|---|
| D1 | `APP_UI_ROUTE` 默认值 | **qss**（不破坏现状，Fluent 用开关打开） |
| D2 | ExWidgets 编不编 | **编但不用**：上游无开关，打补丁不值当；不进安装包 |
| D3 | SARibbon 与 Fluent 的关系 | **各自独立**（混搭是硬约束 2 的反面） |
| D4 | qt-libui 的定位 | **保持独立**（当试验场），两仓不留依赖；只把底座样式表按原样拷回 `src/style/`，此后不再同步 |
| D5 | 项目层 QSS 规则翻不翻 | **不翻**——见下，这个任务本身不成立 |
| D6 | `FluentUI3Style.dll` 进不进包 | **进**（`.iss` 一行 + `skipifsourcedoesntexist`），换 .iss 零分支 |

**D5 的结论（2026-10-13 查证后写死）**：原文说"走 fluent 时把 38 条项目层规则
逐条翻成 Fluent 属性"——**做不到**。`fluentui3styleproperties.h` 只提供 23 个
属性，全是**部件外观开关**（TabBar 样式、进度条样式/粗细、旋钮、滑块提示、
下拉/菜单动画、按钮强调、开关样式、NavigationView、Segmented 配色）；而项目层
那 40 余条规则是"某个对话框/区块的配色、字号、圆角、渐变"。两边没有交集：
`QProxyStyle` 按 Fluent 自己的调色板画标准控件，没有"给某个 objectName 单独上色"
的机制。**fluent 路线的真实代价就是这些品牌化外观不存在**，接受 Fluent 默认观感。
唯一对得上的是底座层角色（`role="primary"/"secondary"/"danger"` →
`ButtonAccentStyleProperty`/`ButtonShapeProperty`），补不补是另一件事。

**风险与解法**：上游 `install()` 会随 `cmake --install` 外溢（Linux 的 AppImage
会把它装进 AppDir；`install(EXPORT FramelessTargets)` 还会因 QWKWidgets 不在导出集
而 generate 报错）→ 作用域内的 `CMAKE_SKIP_INSTALL_RULES`；上游插件会往 Qt 安装
目录写文件 → `BUILD_PLUGIN=OFF`。

## 日志模块拆分（2026-09-30，原《日志模块拆分设计》）

日志核心从 Qt 里拆出来成 `src/logging/`（Qt-free 静态库，只链 spdlog），
`src/core/logger/` 只剩 Qt 桥，**门面同名同签名 → 66 处调用点零改动**。
动因：依赖方向反了（日志核心主动 connect 上层 `ConfigManager`）、每次调用两次
字符串分配、以及 Qt-free 模块链不到日志。

> 原文档写于产品仓语境（`hal`/`measure`/`ephad_log`）。本仓没有 hal/measure，
> 目标名是 `${APP_TARGET_PREFIX}_log`；其余结论原样适用。

**定死的约束**（实现时不许走样）：`ApplyOptions` 必须经 `spdlog::set_level()`
（它遍历 registry，只改全局变量则设置页改级别不生效）、且是**全量重放**（级别 +
两套 pattern）；`extra_sinks` 一律用 console pattern；启动期回放必须在 sink 挂载
之后；`Shutdown()` 先放 sink 引用、再让调用方删 history buffer（否则 sink 里的
裸指针悬垂）；**日志目录由核心建**（`rotating_file_sink` 自己不建目录，失败被
降级路径吞掉，表现为"没有日志文件"这种难查的故障）；模块名用 `string_view` +
带 `std::less<>` 的 `std::map`（C++17 无异构查找，查表不分配）。

**路径必须由桥侧算**：`QStandardPaths::AppLocalDataLocation` 的语义（含
organization/application 两层）不是 `std::filesystem` 能复刻的；核心自己推路径
一旦差一点，日志就悄悄搬家，事后极难发现。

**启动期缓冲**：未 `Init()` 时 `GetLogger` 不返回 nullptr，而是落进同步缓冲，
`Init()` 尾部按原级别回放并加 `[启动期]` 前缀——`SingleInstance` 的判定发生在
`Logger::init()` 之前，那段恰是"程序为什么一闪就退"最需要的线索。已知缺口：
第二实例 early return 走不到 `Init()`，它的日志仍不落盘。

**验证**：两个配置构建通过；`test_log_core.exe` **不把 Qt 加进 PATH 也能跑通**
（核心真的不依赖 Qt）；日志目录没搬家；启动期回放仍有效。

## tour 库重构决策（2026-10-09，原同名文档）

`src/tour` 按一次外部评审重构的决策台账，外加**从代码注释里搬出来的坑**。代码里
只留"为什么要有它"（每条 ≤3 行），"当年那场事故怎么发生的"记在这儿。

**评审四条**：D1 上帝类 `TourOverlay`（759 行）按"谁画什么"拆成
`TourOverlayMask`（高亮在哪儿：挖洞/命中/动画）、`TourBubbleView`（气泡自己）、
`TourCountdown`（还剩几秒）、`TourBubblePlacer`（摆哪儿，纯几何可单测）+ 协调者
`TourOverlay`；D2 恒真死配置 `use_anime_` 删除；D3 高亮三份状态改成"一处真相 +
一个明确标注的动画副本"；D4 `Target` 按内容分型（`WidgetTarget`/`GlobalRect`/
`SplashImage`，额外靶只收前两种 → 图片进不来成了类型保证）；D5 方法小驼峰、
类名统一 `Tour` 前缀。

**坑（按主题，全部仍有效）**

- **状态一个写者**：`setInteractive` 与 `setPassiveMode` 写同一个
  `WA_TransparentForMouseEvents`，后者无条件盖前者——遮罩穿透从来没生效过；
  自动挡布尔被直接置 false——走过实操主题后连讲解主题也不自动翻页；
  "还有没有下一站"曾有两个出口（数组末尾可能是占位主题）
- **几何**：`QRect::center()` 是 `(x1+x2)/2` 向下取整、不是几何中心（摆气泡用它，
  单测按"目标中线"手算的期望值会比几何中线小 1）；多靶的洞必须 `WindingFill`
  （默认 OddEven 下重叠区算洞外，遮罩留黑竖条）；点空白退出要**逐个矩形判、
  不合并并集**（否则遮罩永远关不掉）；锚点矩形只许在 `RectInOverlay` 一处算
  （两处实现漂移 = 同一步两次高亮在不同位置）；`clearHighlight()` 要停动画，
  否则在途动画下一帧把孤框写回来
- **气泡**：换挂法与显隐有三种躲不掉的情况（实操模式下必须提到独立顶层窗口才
  点得到；**被显式隐藏的子控件不会随父 `show()` 回来**；`setParent()` 会隐藏控件，
  换挂法后要显式恢复 + `raise()`）；提到顶层后必须显式删，否则泄漏且窗口留在屏幕上
- **倒计时**：自动挡与"外部驱动的视觉倒计时"两种走法收进 `TourCountdown`
  （`finished()` 只在自动挡发）；起表那一帧必须自己刷状态（归零的是"已过去"的比例）
- **首页与菜单**：`exec()` 等的必须是"用户选完了"，不能等 `hideEvent`（页面没了 ≠
  选完了）；菜单那条主题由宿主**显式传入**，不许 `topics().first()`；标题不许写死
  产品名（默认取宿主 `applicationName()`）
- **样式**：`TourCard` 的自绘曾无条件盖住 QSS → 外观只有 QSS 一个出处；
  `TourStyle` 必须并入**应用级**样式表（实操模式下气泡被 reparent、离开首页子树）；
  `Q_INIT_RESOURCE` 必须在全局作用域；静态库里的 qrc 符号要有人显式引用一次
  - **并入的时机有两条顺序约束**（2026-10-13 实测）：① 在宿主设完应用样式表
    之后（它是"追加"）；② **早于任何窗口级样式表**——`ensureApplied()` 做的是
    重设应用表，排在主窗口的 SARibbon 主题之后会让**功能区主题整个失效**
    （现象是功能区没样式；把引导从"登录前"挪到"窗口之后"时踩到过）。本仓由
    main 在 `Application` 构造之后、任何窗口之前显式调一次，引导控件本身仍
    在登录之后才建
- **funq 桥接（对外契约，脚本与用例按名字寻址，不能改名）**：① 必须是可见的
  QWidget（`call_slot` 走 `qobject_cast<QWidget*>`，且 `client.widget()` 默认等
  visible）；② 必须是独立顶层控件——脚本按 `tourBridge` **单段路径**寻址它，
  挂窗口下路径就多一段；③ 槽签名只能"单个 QVariant 进、QVariant 出"
  （多形参直接 `NoMethodInvoked`）；④ `highlightCount` 必须是**静态**
  `Q_PROPERTY`（funq 只读 metaObject 里的属性，动态属性读出来是 KeyError）；
  ⑤ 锚点 `tourOverlay`、气泡 `bubble_self`（父子关系也不能变）、槽名
  `showHighlight`/`hideHighlight` 等一律不许改名；⑥ 不可见控件的几何是未初始化
  的 → 拒绝高亮并回报原因；额外靶必须与锚点同窗口
  - 路径解析照 `libFunq/objectpath.cpp` 的行为**重写**、不抄源码——funq 是
    CeCILL v2.1（copyleft），拷进来会把 tour 卷进那个许可证；算法本身不受版权保护
  - **2026-10-13 修订**：原先还有一条"必须早于登录就存在"（脚本第一步跑在登录框
    上）。引导已改到登录之后装配（main 的 `SetupTour`），该约束作废，登录框不再高亮

**两条硬约束留给后来人**：改库的公开接口要同步改 `docs/`（每条坑都对应代码里一句
"为什么"，改掉行为却留着注释比没有注释更坏）；提交前过 clang-format。

## 登录示例双路线适配（已过时，2026-10-13，原同名文档）

`examples/new_login_dialog` 是网页版登录页的 Qt 移植，2026-10-14 被策略化重构版
取代并从仓库删除；现实现是 `src/app/dialogs/login/` 与
`src/app/dialogs/login_dialog.cpp`。仍可复用的结论：

- **自绘的是"画"，标准控件的是"皮肤"**：角色面板 / HoverButton 两条路线都用
  QPainter；输入框、复选框、按钮外观随路线走。
- **字号/字重/边距/控件尺寸算版式**（代码 + 布局），颜色/边框/背景算皮肤
  （qss 进项目层，fluent 交给 Fluent）。
- **底座已经声明过的属性必须放回 QSS**：`QLineEdit::setTextMargins` 是叠加不是
  替换，输入框 padding 一半在代码一半在 QSS 会打架。
- **复选框 `::indicator` 要把默认 / `:hover` / `:checked` / `:checked:hover` /
  `:disabled` 写全**：`#id` 具体度高于底座类型选择器，Qt 又逐属性合并，只写一条
  无伪态规则会把勾选态底色一起盖白。
- **fluent 路线不加载任何 QSS**，应用字体在 `setStyle` 之后设，与
  `src/app/application.cpp` 同序。
- `APP_UI_QSS` 宏口径已被"只留 `APP_UI_FLUENT`"取代，见下一节。

## 界面路线开关下沉到目标（2026-10-14，原《界面路线开关下沉到目标设计》）

`APP_UI_ROUTE` 从顶层缓存变量改成普通变量，路线成为目录/目标属性；同一构建
目录可以 demo_app 走 fluent、examples 走 qss。

**仍有效**

- 顶层删缓存 set 与 `set_property(CACHE ... STRINGS)`（后者在缓存项不存在时
  configure 直接失败），保留值域校验。**不用 `option()`**：缓存语义会复活
  "删缓存/旧值静默覆盖"的坑。
- 三层覆盖：顶层默认 qss → `local.cmake`（本机、不入库）→ 子目录 `set`
  （目标级固定，兄弟目录互不可见）。普通变量会**静默遮蔽 `-DAPP_UI_ROUTE`**。
- demo_app 实际路线发布为全局属性 `APP_DEMO_UI_ROUTE`；`tests/funq_app` 的
  `FUNQ_RIBBON` 判据改成 `BUILD_RIBBON AND NOT demo_app fluent`，交叉断言在
  两条路线下都成立。用 GLOBAL 属性而不是 PARENT_SCOPE：tests/ 与 src/app 是
  兄弟目录。
- 宏归一只留 `APP_UI_FLUENT`（fluent 才定义，qss 无宏）。三个不真用 Fluent 的
  示例只注宏、不链 FluentUI3Style，避免死依赖；只有真正 `new FluentUI3Style`
  的示例才链库。
- 迁移期顶层检查 `DEFINED CACHE{APP_UI_ROUTE}`，命中就 WARNING 提示缓存已
  无效，避免旧构建目录静默回到 qss。

**验收**：单目录混编（src/app 临时 set fluent + examples 默认 qss）全量编过、
demo_app 起 Fluent 壳、任一示例起 qss；验完回退那行；默认 qss 的 ctest 与
改动前一致；`grep APP_UI_QSS` 归零（除 CHANGELOG/docs）。

## 登录左栏两支 ASCII 动画（2026-10-14，原《登录左栏补两个ASCII动画设计》）

从 qt-libui 的 just_for_fun 移植 TvStatic（58×26）与 Rocket（48×28），
`(QChar, colorIdx)` 字符缓冲 + 等宽字体逐格画；登录族同时收口到
`src/app/dialogs/login/`。

**仍有效**

- `AsciiSceneAnimator<Scene>` 是**模板壳**：绘制逻辑只一份，底色走构造参数，
  别名只收口类型（`TvStaticAnimator = AsciiSceneAnimator<TvStatic>`）。
- 模板不声明 `Q_OBJECT`：AUTOMOC 不跑；代价是 `qobject_cast`/`findChild` 对它
  不可用，只当 `CharacterAnimator*` 用。
- 场景按原样移植，只做五件事：头保护宏、保留出处注释、补花括号、局部量
  `BODY` 改名、过 clang-format（ASCII 画保留 `// clang-format off`）。
- 配置键 `login/animation`（default/static/tv/rocket），缺省值注册进
  `initDefaultValues()`；构造时读一次，不接 `configChanged`。
- 构造期必须 `scene_.Update(0.0)` + `elapsed_.start()`：前者防首帧空白，
  后者防未启动 `QElapsedTimer` 的未定义值。
- 20fps（50ms）；`reset()` 不覆写，面板只在销毁旧策略时调用。
- 取配置必须传 `QString`：传字面量会推导成 `char[8]`，命中
  `QVariant::fromValue` 的 static_assert。

## 电视动画交互化（2026-10-14，原同名文档）

让 TvStatic 对登录窗的 5 个交互事件作出反应，不动 LoginWindow 与
CharacterPanel：面板本来就有 50ms tick 与 `setInput()`/`onValidationFailed()`。

**仍有效**

- 场景侧用**绝对覆盖 + 滚动相位**，不用偏置：`lock` clamp 到 0 时画面 100%
  雪花，负偏置在雪花相位零像素后果；恒定偏置在锁定平台上只给常量行位移。
  `TvControl{lock_override, roll_extra}` 嵌在场景里，`lock` 由场景自己夹。
- 读得出"在滚"的两个条件缺一不可：卡片结构稳得住（`0.97·lock ≳ 0.35`）且
  真有位移（基础 `roll` 是静态偏移，位移动力只来自 `roll_extra`）。
- 策略侧 `TvStaticAnimator` 用 `tick()` 倒计数（800ms=16 帧、2500ms=50 帧），
  不用 `singleShot` + 世代号；面板隐藏时定时器自停、倒计时挂起，连点不叠链。
- 顺序：递减 → 推进 `roll_extra_`（`fmod 17`）→ `ApplyControl()` →
  `SetControl()` → 再调基类 `tick()`（基类一进门就 `Update()`，晚调滞后一帧）。
- 偷看按 `pwd_visible && has_pwd` 的**上升沿**起 800ms，判定必须在调基类
  `setInput()` **之前**，否则 `input_` 已更新、上升沿永远不成立。
- 映射定稿：打字 lock 0.45、聚焦 0.80 + 0.35 行/帧、偷看 1.0 锁 800ms、
  错误 0.40 + 0.8 行/帧 2500ms；错误档 0.30 时卡片可见格 29%，低于读数线。
- 验收只能抓图：策略是普通 QObject、面板不暴露动画状态；判据用"可辨认形态"
  （满卡 / 四成碎卡+撕裂 / 八成卡慢跑 / 四成卡+六成雪+滚屏），不用"与基线
  不同"。帧间距 ≥500ms；先归一化穿梭，避免持续态污染期望值。

## 内置 Markdown 文档预览（2026-10-14，原同名文档）

新增 `src/markdown/`（`demo_markdown`）：md4c 解析 + qlitehtml 渲染，
控件 `MarkdownView` + 独立查看器窗口；文档从磁盘读，改完即看。

**仍有效**

- 边界在 `MarkdownView`：qlitehtml 类型不进公开头，链接只抛 `linkClicked`，
  本地 .md 自开/其余交系统浏览器是窗口策略。
- 渲染契约：md4c 必须用 `MD_DIALECT_GITHUB`（否则表格静默丢失）；
  `setUrl` 必须早于 `setHtml`，空基准也要显式设 `QUrl()`，否则相对图片按上一份
  文档解析。
- 热重载用有界轮询（100ms × 50 次）而不是单次重试；"先删后建"保存时也能等到
  文件回来；`addPath()` 失败要抛 `loadFailed`。
- `LoadFile` 失败要 `ClearDocument()` 并回错误页，不能只改标题。
- 页内锚点先看 `fragment()`：纯 fragment 只滚锚点，不能走
  `QDesktopServices::openUrl`（基准 URL 是目录，会弹资源管理器）。
- 只放行 http/https/mailto 与真实存在的本地文件。
- 单测：`test_markdown_to_html`、`test_code_highlighter`；像素探针
  `scripts/funq/probe_markdown_view.py`。

**当时定案、后来被后续工作覆盖**

- 初版"零补丁"已被 qlitehtml 0001-0006、0008 取代。
- 初版"不做暗色主题"已被 Light/Dark 主题接入覆盖。
- `markdown_window.{h,cpp}` 已迁到 `src/app/dialogs/`，模块边界仍是
  `src/markdown → src/app` 单向。
- 备查未做：把"解析自己生成的 HTML"换成 md4c `MD_PARSER` 回调，可根除
  `<pre><code>` 正则吞正文的同源问题；改动面大，等真有需求再上。

**坑**

- 只 `add_subdirectory` qlitehtml 的 `src/` 会丢顶层 `CMAKE_AUTOMOC ON`，
  链接期报一屏 `staticMetaObject` 找不到。
- `Q_INIT_RESOURCE` 放进匿名命名空间会绑到匿名空间、链接期找不到 rcc 函数。
- md4c 不 `add_subdirectory` 的真陷阱是它改全局编译设定（强推 `/MT`、覆盖
  build type），不是"没有测试目标"。

## 公式/图表图片 QPixmap 直供（2026-10-20，原《Markdown公式图表QPixmap直供设计》）

qlitehtml 增加 `PixmapCallBack`（补丁 0008）：宿主已有解码好的 QPixmap 时
直接提供，避免每次 `setDocument()` 走 base64 + PNG 解码；打印容器同步复制
回调。

**仍有效**

- `load_image` 先查 provider，返回空再走 DataCallback；provider 图按静态图
  处理，跳过 `loadAnimation`。
- `setDocument` 保留 `m_pixmaps.clear()`：跨文档复用会让动画图不再注册
  QMovie、本地图片内容变化后仍命中旧图；缓存放宿主。
- markdown 侧 token 用 `mj:f/<sha256>` / `mj:m/<sha256>`，由渲染器内容缓存
  key 派生；不能按 ApplyDocument 递增，缓存命中不会重新产出 QImage。
- 两层缓存：`MarkdownRenderContextState::pixmap_cache` 跨 ApplyDocument 复用，
  `MarkdownView::document_pixmaps_` pin 住当前文档；provider 只读 pin 层，
  避免 QCache 淘汰当前文档条目造成 token 悬空。
- `QCache::insert` 失败会删除对象，失败后不能再用 entry 指针；pin 层仍保证
  当前文档可用。
- `ClearRenderCaches()` 清跨 ApplyDocument 缓存，不清 pin；PDF 导出走
  `print()` 的 provider 副本。
- 解码上限：PNG 字节与像素字节均 64MiB，单边 32767；公式侧补了与 mermaid
  相同的 4096² 面积封顶。
- 只覆盖公式/图表；本地图片仍每次 `setHtml` 读盘解码。

**坑**

- provider 必须在第一次 `setHtml()` 前设置：litehtml 在 `createFromString()`
  的 `compute_styles()` 阶段就会对每个 `<img>` 调 `load_image`。
- 不能删 `m_pixmaps.clear()`（见上）。
- 不能只复制 dataCallback 到打印容器，否则 PDF 缺图。
- `mj:` 未命中不要在 qlitehtml 内统一告警：本地图片/真 data URI 返回空是正常
  回退；告警放宿主，只对 `mj:` 打。
- QImage → QPixmap 必须在 GUI 线程；不要给 pixmap 设 `devicePixelRatio`
  （qlitehtml 用显式目标矩形绘制）。

## 挂起事项（原 `挂起事项.md`，活清单）

记录**已知但未处理**的问题与待办，每条都带证据或复现方式。做完一条就删掉这条。

### 1. ~~SIGTERM 杀不掉应用~~（已了结：定位并修复，2026-10-15）

**现象**：WSL 里 `timeout 10 <AppImage>` 启动，超时后进程仍存活——实测有一个实例
残留 5 分 54 秒，并在 WSLg 桌面上留下无法操作的窗口。

**结论**：排除了原先列的可能①——这次是**直接执行可执行文件**（不经 AppImage）
复现的，根本没有 `AppRun` 可怪。坐实的是可能②：
`GlobalExceptionHandler::signalHandler` 末尾 `std::raise(signal)` 重发时**没有
复位处置**，而 POSIX 的信号处理函数是常驻的，raise 于是重入自己 → 递归到栈溢出
都不退出，日志被刷爆（实测 10 分钟 269MB、299 万行、时间戳全在同一毫秒）。
用户点窗口关闭按钮发的正是 SIGTERM，所以表现为"窗口关不掉"。

**修法**（提交 `7fddeb2`）：重发前 `std::signal(signal, SIG_DFL)`。回归用例
`tests/core/global_exception_handler_test.cpp`（`test_core_geh_term`）：父进程
拉起子进程 → 子进程装好 GEH 后 raise → 断言它必须在超时内死掉。

**判别力在 Linux 一侧**：MSVC 的 CRT 在处理器返回后会终止进程，所以 Windows 上
不复位也照样退出、用例恒过；只在 POSIX 上能验出这个 bug——这也是它长期没被
发现的原因（Windows 侧天天在跑，Linux 侧没人掐过进程）。

### 2. GEH 与 SingleInstance 无测试

**现状**：两个模块都没有测试覆盖——`GlobalExceptionHandler`（Qt 消息重定向、
SIGINT/SIGTERM）、`SingleInstance`（第二实例转发参数并激活首实例窗口，非 Windows
走 `showApplication` 信号那条路）。

**已有旁证**：进程被 SIGTERM 杀掉后，单实例痕迹（QLocalServer 套接字、
QSharedMemory 段）会残留，导致下一次启动静默转发参数并以 0 退出——日志为空，
因为它退在 `Logger::init()` 之前。这条旁证同时指向第 1 条。

**入手点**：第二实例的行为可用现有子进程骨架测：父进程起首实例 → 再起一个 →
断言第一个收到激活请求且第二个以 0 退出。

### 3. 崩溃栈符号链路无测试

**现状**：符号归档的两条通道都已就绪（Windows `make_symbols` 归档 exe + PDB；
Linux `make_appimage` 归档未 strip 的可执行文件），但没有测试锁住"事后能解析出
调用栈"这条链。

**已验证到哪一步**：Linux 侧实测归档 25.8 MB / 3579 个符号 / 带调试信息，而
AppImage 内是 0.96 MB / stripped / 0 个符号，两者对照成立。但**没有人真的拿崩溃
日志里的地址跑过一次 `addr2line`**，Windows 侧也没有解析过 minidump。

**入手点**：在崩溃子进程测试里，从生成的崩溃日志中取一个地址交给
`addr2line -e <归档二进制>`，断言解析出的符号名非空；Windows 侧用归档的
exe + PDB 解析 minidump。

### 4. ~~待你拍板：`scripts/make_appimage.sh` 是否入库~~（已了结：前提是错的）

原判据是"Windows 侧的同类生成物是入库的，所以两平台约定不一致"。**实测否定**：
`git check-ignore` 逐个查过，`scripts/make_installer.bat`、
`scripts/make_install_package.iss`、`scripts/make_appimage.sh` **三个都被忽略**，
全是 configure 生成、入库没有意义的产物。结论：约定本来就是一致的，没有要拍的
板。（2026-09-29 做 funq 随包分发时顺手核的。）

### 5. tour 库的告警通道：`qWarning` 还是 `Logger` 宏

**现状**：`tour_bridge.cpp:128`、`tour_catalog.cpp:12/16/39/43`、
`tour_controller.cpp:98/105/122/131`、`tour_style.cpp:46` 共 9 处用
`qWarning()`，而项目规则要求尽可能走封装的 `Logger` 宏。

**卡在哪**：`src/tour` 的依赖只有 `Qt5::Gui/Widgets/Core`——它**不链 core**，
而且刻意如此（库可整包拷到别的工程用）。要用 `LOG_*` 就得把 core 拖进来，会破坏
这个卖点。两条路：① 链 core 换 Logger 宏（统一日志，代价是库不再零依赖）；
② 保留 `qWarning`，把"为什么不链 core"写成注释，免得下次又被当缺陷报一遍。

### 6. 待你拍板：tour 的告警文案与链式构造器 API 风格

`tour_catalog.cpp:12/16/39/43` 的告警文案是 `"foolish! step is nullptr!"`
一类，措辞不像本仓手笔，疑为外部移植残留；`withTitle` / `withStep` / `addTopic`
那套链式构造器 API 与本仓其余部分的风格也不一致。保留（尊重上游写法）还是改写
（与本仓风格统一，涉及库的公开 API），需要定。

### 已决定不做

- **libui 冻结**：`src/libui` 的标签栏外观与进度条控件不再扩展。它们是学习性质
  的绘制练习，9 种外观 / 4 种进度条已超出模板所需。**该判断已由后续动作落实到底**：
  整个 `src/libui` 先抽为独立仓库、以 submodule 内嵌，后又整包移除——本仓现在不含
  任何 submodule，也不再用该库；仅 `SwitchButton` 源码内联进 `src/app/widgets/`
- **Linux 打包边界**：只做 `configure + build + test + AppImage`，不移植
  deb/rpm 等发行版打包
