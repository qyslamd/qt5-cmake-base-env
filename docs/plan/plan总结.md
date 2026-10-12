# plan 文档归档总结

`docs/plan/` 只留这一份：已收尾的方案提炼到这里后删原文，原文用
`git log --diff-filter=D -- docs/plan/<文件名>` 查；未处理的问题作为活清单
放在最后一节。新写方案时在 `docs/plan/` 下另起文件，收尾后再并进来。

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
> [界面双路线与FFT选型设计](界面双路线与FFT选型设计.md) 的 D4。

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
  默认还要等 `visible`）；**必须早于登录就存在**（脚本第一步跑在登录对话框上，
  那时主窗口还没建）→ 独立顶层窗口（`Qt::Tool` 不进任务栏）；**签名必须是
  "单个 QVariant 进、QVariant 出"**（funq 固定按那种 `invokeMethod` 调，多形参
  直接 `NoMethodInvoked`）
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
- D3 `--crash` 由 `APP_ENABLE_CRASH_TEST` 控制（模板默认 ON，发布可关）
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

## 挂起事项（原 `挂起事项.md`，活清单）

记录**已知但未处理**的问题与待办，每条都带证据或复现方式。做完一条就删掉这条。

### 1. SIGTERM 杀不掉应用

**现象**：WSL 里 `timeout 10 <AppImage>` 启动，超时后进程仍存活——实测有一个实例
残留 5 分 54 秒，并在 WSLg 桌面上留下无法操作的窗口。

**影响**：用户 Ctrl+C、服务停止、CI 结束作业时进程不会退出。

**待分辨的两种可能**（需分别验证，不要直接猜）：① AppImage 的 `AppRun` 包装没有
把 SIGTERM 转发给子进程；② `GlobalExceptionHandler` 的信号处理没有真正终止进程。

**线索**：`src/core/crashhandler/global_exception_handler.*`；GEH 的职责包含
SIGINT/SIGTERM（见本文件归档的 D6），即这是**声称已覆盖但实际未验证**的能力。

**入手点**：先补一个子进程测试区分上述两种可能——现有
`tests/core/crashhandler_test.cpp` 的"父进程拉起子进程"骨架可直接复用。

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

### 7. funq 手动示例的"有登录框"那条分支没在本仓实跑

**现状**：`b1f0db2`（`fix(funq): 解释器选择收敛到一处…`，自产品分支 cherry-pick）
让示例脚本先探测登录框：有就走完整七步，没有就跳过登录三步、从主窗口继续。
**没有登录框那条路已在 x86 relwithdebinfo 上实跑过**（client 退出码 0，七步全过，
2026-09-29）；而本仓登录是开着的——**有登录框那条路一次都没跑过**。

**入手点**：在本仓构建一次（`APP_ENABLE_AUTH` 默认 ON），按
`examples/funq-scripts/README.md` 起两个终端，确认第 2-4 步（输错密码 →
看提示文字 → 换对密码进主窗口）照旧。跑过就把这条删掉。

### 已决定不做

- **libui 冻结**：`src/libui` 的标签栏外观与进度条控件不再扩展。它们是学习性质
  的绘制练习，9 种外观 / 4 种进度条已超出模板所需。**该判断已由后续动作落实到底**：
  整个 `src/libui` 先抽为独立仓库、以 submodule 内嵌，后又整包移除——本仓现在不含
  任何 submodule，也不再用该库；仅 `SwitchButton` 源码内联进 `src/app/widgets/`
- **Linux 打包边界**：只做 `configure + build + test + AppImage`，不移植
  deb/rpm 等发行版打包
