# 更新日志

本文件记录各版本的主要变更。

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循[语义化版本](https://semver.org/lang/zh-CN/)。版本号由 git
派生——tag 提供前三段、提交数提供 build 号，详见[发布指南](docs/guide/发布指南.md)。

## [Unreleased]

### 新增

- **接入 FFTW，新增频谱示例 `examples/fft_demo`**：FFTW 3.3.11 走本仓其他源码
  依赖同样的路子（tarball 入库 → configure 解压 → 打补丁 → `add_subdirectory`），
  静态、单精度（`fftw3f`）、线程合并进主库。**六个值得记的点**：① `BUILD_TESTS`
  默认 ON，会 `enable_testing`、把 32x64/ib256 这些 bench 用例塞进我们的 ctest
  还多编一个 bench 工具——上游给了开关，用 `set(... CACHE BOOL ... FORCE)` 钉掉
  （zlib 那份是同一件事但它没给开关，所以只能打补丁）；② `cmake_minimum_required
  (VERSION 3.0)` 在 CMake 4.x 下直接配置失败，`patches/fftw/0001` 抬到 3.15；
  ③ FFTW 只登记了 `$<INSTALL_INTERFACE:include>`，树内消费者拿不到头文件路径，
  但**不能**给 `fftw3f` 补 INTERFACE 目录——上游有 `install(TARGETS ... EXPORT)`，
  导出目标的接口里出现源码树路径会在 generate 阶段报错（实测），所以包含目录由
  消费者自己加，与 crashpad 同款；④ `BUILD_SHARED_LIBS` 是 CMake 通用变量、后面
  还有依赖在读（QtTesting / SARibbon / zlib），所以用**普通变量**在
  `add_subdirectory` 前后夹住而不是写进 cache——补丁把 FFTW 的 policy 抬到 3.15
  之后 CMP0077 是 NEW，`option()` 不覆盖同名普通变量，这三件事都实测过；
  ⑤ x64 下 `HAVE_SSE2` 探测不到（`/arch:SSE2` 不是合法的 x64 选项，SSE2 本就是
  基线），但 AVX / AVX2 正常打开，x86 下三个都开（Linux 侧三个都探测得到）；
  ⑥ **上游一处只在 GCC 侧炸的 CMake 缺陷**：它把 AVX2 与 FMA 两个编译标志分写成
  两句 `set_source_files_properties`，而两句写的是同一个 `COMPILE_FLAGS`——后一句
  覆盖前一句。GCC 侧探测得到 `-mfma`，于是 avx2-128 的源文件只带 `-mfma` 去编，
  撞上 `simd-avx2-128.h` 里「GCC 且未定义 `__AVX2__` 就 `#error`」的守卫，构建
  直接失败；MSVC 侧侥幸躲过（`/arch:FMA` 不是它的合法选项，探测失败、覆盖没发生，
  所以它一直是正确的 `/arch:AVX2`）。`patches/fftw/0002` 把两句合成一句、两个
  标志都给上。示例本身是一条完整链路：
  合成信号（三个可调分量 + 噪声）→ 加窗（矩形 / Hann / Hamming）→ 单精度 r2c →
  **三种谱视图**（单边幅度 / 双边幅度 / 单边 PSD dB·Hz⁻¹，切换即重算，因为那是
  视图不是信号）→ **插值找峰**（报出各分量的频率与幅度，频率细化到 bin 以内）→
  **频域阈值去噪 + 逆变换**（`c2r`，时域图上叠一条去噪后的波形）。三个归一化口径
  在一个文件里对齐：幅度谱除窗之和、PSD 除窗的平方和与采样率、逆变换自己补 1/N
  （FFTW 的 c2r 不做归一）。数值用一次性探针逐项核对：bin 上的分量峰值精确等于
  幅度、双边谱在 ±f 各半且关于 0 对称、**PSD × bin 宽 = a²/2**（口径与物理量对得
  上）、噪声底实测 −59.4 与理论 −57.9 dB/Hz 差 1.55 dB（正好是中位数相对均值的
  ln2 因子）、非 bin 分量 100.4 Hz 被插值到 100.36、去噪后与真值的 RMS 从 0.2104
  降到 0.1760。过程中抓出三个错：**use-after-free**（原先在 `fftwf_free` 之后才读
  输出数组，Debug 下峰值能到 1e15，Release 下则常常"看着是对的"）；**找峰门槛
  只比中位数**（无噪声时中位数≈0，矩形窗那些衰减到 −190 dB 的旁瓣被全报成分量，
  64 个）→ 补一条"不低于最强谱线的 5%"；**把未归一的 `|X|` 拿去报峰值幅度**
  （差 N/2 倍，而峰的位置不变，所以这个错只错一半、图上看不出来）
- **中文化第三方控件的自带文案**：本仓自己的文案一直是硬编码中文，于是界面上
  唯一的英文来自第三方——功能区的标题栏按钮提示（`Maximize` / `Restore`）、
  标题图标右键菜单（`Restore(R)` / `Move(M)` / `Close(C)` …）、以及 Qt 自带
  对话框的按钮。**上游其实带了中文 .ts**（`SARibbon 2.5.7` 的
  `src/SARibbonBar/i18n/SARibbon_zh_CN.ts`，实测 lrelease：38 条已译、11 条
  未译且全是开发者警告），问题在于它从没被编译、更没被加载——SARibbon 自己的
  CMake 只把 `.qm` 挂成静态库的构建产物，它的 qrc 与代码里都没有 `QTranslator`，
  加载得由宿主程序做。做法：`.ts` 拷进 `src/app/translations/` 由本仓持有，
  构建期 `qt5_add_translation`（只 lrelease、不 lupdate，避免构建回写源码）编成
  `.qm` 落构建目录，用 `configure_file` 生成 qrc 把它嵌进 exe，`main.cpp` 在
  **任何界面构造之前**装 `QTranslator`——SARibbon 的文案多数在构造期就取走了。
  Qt 自带那份（`qt_zh_CN.qm`）走文件部署而不是嵌入，并且**得自己拷**：
  windeployqt 是遍历 Qt 翻译目录里的 `qtbase_*.qm` 用 `lconvert` 拼出
  `qt_<语言>.qm`，而 5.15.2 的安装里没有 `qtbase_zh_CN.qm`（只有 `qtbase_zh_TW.qm`），
  现成的 `qt_zh_CN.qm` 因此永远不会被它拷出来（实测 deploy 出 23 个语言、独缺
  zh_CN）。两个平台都实测装载成功（Windows 从 `bin/translations/`，功能区那份
  从 `:/translations/`）
- **崩溃报告回传链路**：程序发布给离线客户后，崩溃文件躺在数据目录里没人去
  翻，链路就断在最后一步。现在崩溃后**下次启动**会弹「上次运行发生过崩溃」，
  点「导出报告」把 `<uuid>.dmp` + 人可读 `summary.txt` 导出到桌面
  `CrashReport-<时间戳>/`，客户把文件夹发回来即可。另有两个无界面入口：
  `--export-report [--export-dir <目录>]`（售后远程指导与自动化演练，不写目录
  则导出到桌面的 `CrashReport-<时间戳>/`）与安装目录下的
  `export_crash_report.bat`（主程序是 WIN32 子系统，cmd 读不到退出码，由它用
  `start / wait` 兜住）。导出**不删除**报告，只调 `SkipReportUpload` 转入已交付
  状态——不再提示，但客户弄丢文件也还能再导一次。注解补上 git 提交与构建配置，
  崩溃报告因此能自证"来自哪个构建"，与 `dist/symbols/<版本>-<git 提交>-<架构>/`
  的归档一一对上。实测两平台（Windows 用 `cdb` + PDB，Linux 用 `lldb` 直读
  minidump）都能从回传的 dump 恢复出"函数名 + 文件:行"；Linux 侧的多层调用链
  也能完整展开，不依赖帧指针（`-fno-omit-frame-pointer` 不必加）。
  已知边界：Crashpad 固定写 `MiniDumpNormal`，dump 里没有全局变量与堆对象内容
- **样式表分两层，底座层独立成模块**：`src/style/`（目标 `demo_style`）带
  `base/*.qss` 十份表与 `ApplyStyleSheet()`，只写类型/伪状态/子控件/属性选择器、
  不出现 objectName，别的程序链上模块调一次就有；`src/app/resources/styles/
  default.qss` 退回项目层，只留本仓业务控件的 objectName 规则。原先那份表 704 行
  里滚动条/下拉框/微调框/分割器占了 65 个规则块，而 QLabel、QCheckBox、QMenu、
  QTableView 等 25 个常见控件连一条裸类型规则都没有。"同角色多控件"改用动态属性
  表达（主按钮、表单输入、报错文本三类合并成 `role="primary"/"form"/"error"`），
  objectName 一个没删——funq 的控件路径与 QtTesting 的录制都靠它。全局字体改走
  `QApplication::setFont`，删掉了 `* { font-family }`。新增 `examples/style_demo`
  按控件族分六页眼验底座层（demo_app 只有业务控件，看不到表格、滑块这些）
- **功能区/菜单栏的动作配上彩色 SVG 图标**：日志窗口、设置、关于、退出四个动作
  各一张（`src/app/resources/icons/svg/`，48×48 扁平彩色，与既有的 `monitor.svg`
  同一套配色）。此前功能区形态下是四个纯文字大按钮，很空。图标设在 QAction 上，
  于是功能区与菜单栏两种形态一起有。SVG 由 Qt 的 svg 图片插件渲染——开发机随
  Qt 安装、发布包由 windeployqt 收进 `imageformats/qsvg.dll`、Linux 在
  libqt5svg5 里；实测四张都能渲出非空 pixmap。`settings.svg` 原先是单色且无人
  引用，一并换成彩色版本

- **Ribbon 界面：SARibbon 作为基础设施接入**（`BUILD_RIBBON`，默认 ON）。
  主窗口的基类随开关二选一：开着是 `SARibbonMainWindow`（菜单栏那一行换成
  功能区），关掉退回 `QMainWindow` + 普通菜单栏。两种形态**共用同一批
  QAction**，差异全收在 `mainwindow.cpp` 的 `initRibbon` / `initMenuBar`。
  功能区下挂三个面板（工具 / 帮助 / 文件），面板都设了 objectName——引导与
  funq 定位的是控件，而菜单项是 QAction，指不了。顺带接上无边框窗口：它的
  frameless 依赖 QWindowKit，而 QWindowKit 要求 Qt ≥ 5.14，所以 Windows 侧
  （5.15.2）启用、Linux 侧（Ubuntu 20.04 的 5.12.8）整个跳过，Ribbon 本体
  不受影响。三个包（SARibbon 2.5.7 / QWindowKit 1.5.0 / qmsetup）按本仓既有
  约定只入库 tar.gz，解压产物不入库。实测（Windows，`--play` 空脚本拉起
  主窗口后读日志）：三个面板都能被 `findChild` 按名找到，`:/SARibbon/...` 的
  图标与 `:/SARibbonTheme/...` 的主题资源均在位——后者是静态库 qrc 的老问题，
  靠 `main.cpp` 里的 `Q_INIT_RESOURCE(SARibbonResource)` 显式引用

- **录制改用库自带的控制面板，校验模式可用**：原先自己弹 `QFileDialog` 拿文件名、
  再调 `recordTests(文件名)`——那条路会**绕过** `pqRecordEventsDialog`，连带把
  「校验模式」（鼠标划过的控件显示绿/红框）与暂停/继续一起绕过去。现在走
  `recordTests()`，面板出来。两个坑记在文档里：文件名仍得宿主先给（那个面板不带
  文件选择）；`recordTests()` 不能紧接着 `setActiveWindow()` 调用——库用
  `QApplication::activeWindow()` 决定要不要弹面板，而刚关掉的文件对话框还没把激活
  交还回来，得延后一轮
- **回放支持 `--slow` 放慢**：`demo_app --play sb.xml --slow 500`，每条事件之间
  等 500 ms，方便人看着回放一步步走。默认是 100 ms（编译期量
  `QT_TESTING_EVENT_PLAYBACK_DELAY`），十几条事件的脚本三四秒就跑完，人眼跟不上。
  走的是 `pqEventDispatcher::setEventPlaybackDelay()`——库公开的静态 setter，
  内部是 `processEventsAndWait`，等待期间照常处理事件、界面正常刷新。
  **不要往脚本里插 `pause` 事件**：那个命令的实现是 `SleeperThread::msleep`，
  头文件原话 "no events will be processed and the application will appear frozen"，
  等待期间界面冻住，而且插了延时就得维护第二份脚本。详见
  [模板使用指南](docs/guide/模板使用指南.md) 的「放慢回放」

- **引导接入 funq：脚本执行的每一步在屏幕上看得见**：把引导的演示层当成 funq 的
  可视化层——脚本走到哪个控件，遮罩就高亮到哪个，同一套渲染层平时给用户当新手
  引导、跑脚本时给自动化当"看得见的光标"。实现上给演示层加了**被动模式**（按钮
  全隐、倒计时一律停、点遮罩不退出，推进全由外部给）与气泡开关，并给
  `GuidanceBridge` 按 path 定位控件的两个 `Q_INVOKABLE`（**零 funq 依赖**：
  自己用 Qt 的 `findChild` 解析、用引擎同一套 `mapToGlobal` 算矩形，所以 guidance
  库不必链 funq）。实测撞出四处约束，都记在 `docs/plan/guidance接入funq.md`：
  桥接对象必须是**可见的 QWidget**（funq 的 `call_slot` 要 `qobject_cast<QWidget*>`，
  纯 QObject 判 `NotAWidget`）、必须**早于登录**就存在（脚本第一步跑在登录框上，
  那时主窗口还没创建，所以它是独立顶层窗口）、槽签名只能是"单个 `QVariant` 进、
  `QVariant` 出"（funq 的 `invokeMethod` 固定按这个调，多参数直接 `NoMethodInvoked`）、
  解析器只认路径最后一段的 objectName（指不了没名字的控件，也指不了菜单项——那是
  `QAction` 不是 QWidget）。另有两条行为规则：高亮目标**就是宿主窗口本身**时遮罩
  往外扩一圈（否则高亮框等于整块遮罩，阴影会被整块掏空、屏幕上只剩气泡），扩出去
  的部分按屏幕可用区裁掉，宿主占满屏幕时自然就不扩；目标不可见或尺寸为空时**拒绝
  高亮并回报原因**（菜单栏里的 `QMenu` 从没显示过，几何是未初始化的，画出来会把
  遮罩铺到屏幕角落去）
- **功能引导模块 `src/guidance/`（静态库）**：把界面导向与操作高亮做成可复用的
  独立库——`GuidanceController` 驱动演示层（半透明遮罩 + 目标高亮 + 气泡 +
  倒计时），主题由使用方定义（Flow/Step，目标可以是**控件、一块区域或一张图**）；
  实操式主题下遮罩对鼠标穿透，用户可边看边动手。库自带 qrc 与样式表，并在
  **应用级**并入样式（追加而非覆盖，不动宿主已有的样式表）——气泡在实操模式下
  会被 reparent 到 viewport，只设在某个控件上的样式盖不到它。静态库的 qrc 符号
  需要有人显式引用，这一步收在库内部（`GuidanceStyle::EnsureApplied()`），宿主
  不必知道资源名——漏写的代价是静默丢样式。外部驱动分两个类各归各位：
  `GuidanceBridge`（必须是可见 QWidget + 独立顶层窗口）管被动高亮，
  `GuidanceMenu`（普通 QObject）给主窗口宿主挂菜单。移植进来时修掉了四处缺陷
  （跨主题自动模式不同步、实操主题被自动翻页、倒计时初值用反、第二遍运行气泡
  不显示），逐条记录在 `docs/plan/plan总结.md`（原文已归档，该文件已删除）
- **引导演示示例 `examples/guidance_demo/`**：一个普通 QWidget 窗口（没有
  QMainWindow、也没有菜单栏——库的核心 API 对宿主是什么窗口没有要求）。演示
  三条主题：讲解式的控件导览、遮罩穿透的实操式主题、以及 QRect / QPixmap 两种
  目标。实操那条刻意夹在中间，跑一次「全部演示」即可看出跨主题时自动模式该降
  该升
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

- **补丁生成器入库（`patches/tools/`）**：`patches/` 下 16 个补丁原先只有两条链
  留有生成脚本，且它们散在 `temp/` 里、把仓库路径写死成 `D:/trae_workspace/...`。
  连同 `temp/` 的清理一起收进仓库：`make_saribbon_patches.py`（SARibbon 四条 +
  QWindowKit，`OUT_QWK` 已备好但目前无 QWindowKit 补丁）与 `make_pick_patch.py`
  （funq 0008）。两者的 `REPO` 改为从 `__file__` 上溯两级推导，docstring 补上
  pristine 工作目录的重建步骤——工作目录本身是 `3rdparty/*.tar.gz` 的普通解压
  产物，删掉不留隐患。`patch_dependencies.cmake` 头部指到这里
- **`temp/` 从 97 MB 清到 893 KB**：清掉的是历次会话的工作残渣——约 40 个
  构建/ctest/cdb 日志、崩溃回传演练的 `crash_export*`、补丁机制的试验副本
  （`applytest{,2,3}` + `ribbon-pristine` 共 54 MB，都能从 tar.gz 重解压）、
  三张截图、当时的改动快照压缩包，以及 libui 时代的绘制试验源码。28 个
  `wsl_*.sh` 一并清掉：它们全部指向 `$HOME/eph205-linux-verify` 与
  `/mnt/d/sb/eph205/workspace/eph205-qt`，实测两个路径都已不存在，脚本已无
  落脚点。留下 4 项：`qss_stats.py` 与 `qss_probe/`（被「通用样式表设计」点名）、
  `build_noribbon.ps1`、`wsl_kill_apps.sh`。c-cmake-base-env 的消费者工程与
  构建脚本挪到它自己的仓（`/d/trae_workspace/c-cmake-base-env/temp/`），不再
  寄存在本仓
- **移除 `qt-libui` 依赖，本仓不再含任何 submodule**：`external/qt-libui`
  是当初把 `src/libui` 抽出去后的回填，本仓实际只用到它的三处，为这三处
  背一个跨仓 submodule 不划算——每次 clone 都要 `--recursive`、改一行库代码
  要跨两个仓提交、pin 的提交还不被主仓的构建验证。三处逐一处理：
  **① funq 的测试靶子**换成新的 `examples/funq_target`（三页签小窗口，
  文本/下拉/微调/复选/进度条/列表/按钮齐全，控件全带 `objectName`），
  测试目录随 `git mv` 从 `tests/gallery_funq` 改名 `tests/funq_demo`，
  用例名 `test_gallery_funq` → `test_funq_demo`。新靶子的路径就是
  `窗口::控件`，不必再从运行中的进程里探（旧 gallery 的控件没有 objectName，
  路径由「类名 + 同名兄弟序号」拼成，界面一动就得重探）；旧用例只能把属性
  原样读回来，新用例多了跨控件联动与真实点击这两类可断言的行为。
  **② 启动画面（splash）删除**：它靠 `SwitchButton` 之外的 libui UI 撑场面，
  而模板的立场是"删掉比留着更好"——启动画面不属于模板要提供的能力。
  **③ `SwitchButton` 源码内联**进 `src/app/widgets/`（源自 qt-libui 6e9fdba，
  头文件里注明），仍是设置对话框在用。踩到一个坑：libui 的 AUTOMOC 把
  所有 moc 汇进单个 `mocs_compilation.cpp`，只要 `demo_app` 还链 libui，
  它的 `SwitchButton::staticMetaObject` 就会被一起拉进来，与本仓内联的那份
  撞成 LNK2005/LNK1169——所以内联与解链必须同步做完（这也是删 splash 的
  顺带收益：删完 `demo_app` 对 libui 再无引用）。完成后 `external/` 整个
  目录消失，README 与各文档里的 submodule 克隆说明一并撤掉
- **顶层 CMakeLists 的 8 处开关集中到「构建开关区」**：此前 `option()` 与
  `APP_CRASH_BACKEND` 散落在各功能段落（最远的 `BUILD_TESTS` 在 440 行附近），
  现全部集中在 `project()` 之后（71–180 行），各自的说明注释跟着走，原位置只留
  一句指引。**两处受约束的分发块刻意留在原位**并写明原因：
  `add_compile_definitions(...)` 是目录作用域且必须先于 `add_subdirectory(src)`；
  `include(cmake/crashpad/crashpad.cmake)` 必须后于 3rdparty 解压步骤。验证时
  踩到一个 CMake 行为：`-DBUILD_TESTS=OFF` 重新 configure 后，旧构建目录里
  上一轮 ON 时生成的 `CTestTestfile.cmake` 不会被清掉，`ctest -N` 读它显示测试
  还在——生成物只增不删，判定开关是否生效要看新 configure 的产物（全新目录
  实测 OFF 完全生效）。另：切开关组合走 CMakePresets.json 的 `cacheVariables`
  即可（`build_ninja.bat` 本就 `--preset` 调用），无需给脚本加 `-D` 透传
- **`external/c-cmake-base-env` 移出主仓**：它是伴随开发的独立 C 基础环境
  （`eph_log_crash`），主工程从不引用它——不 `add_subdirectory`、不链接、代码里
  零引用，只在 README 的目录树里挂了个名字。留着它的代价是：每次 clone 都多拉
  一个用不到的仓；它每改动一次主仓要跟着 bump pin；而主仓构建又不会验证它
  （pin 一个不再验证的东西最容易悄悄腐烂）。改为独立仓库单独 clone 开发，需要
  时并列放即可——`git submodule` 三项收尾（`deinit` / `rm` / 清 `.git/modules`
  缓存）都做完了，仓本身在 gitee 上完好。`qt-libui` 当时不受影响，仍在
  `external/` 下随本仓一起编译（后来也移除了，见上一条）
- **删掉 4 对从未使用的异常类，`GlobalExceptionHandler` 归位到 `crashhandler/`**：
  `common/{file,time,string,byte}_exception.*` 共 263 行，自 initial commit 起
  全仓零引用（只有 CMake 源清单与它们自己的配对）——而且与本仓惯例冲突：
  `src/` 里一个 `throw` 都没有，错误处理一律"返回 bool/状态 + `LOG_*`"，摆着
  一套没人抛的异常体系只会误导模板使用者。`global_exception_handler.*` 则移进
  `crashhandler/`：它 85% 的篇幅是信号兜底，且与两套崩溃后端有明确职责划分
  （自研模式下 CrashHandler 接管崩溃信号、它退为二线；Crashpad 模式下它只留
  SIGINT/SIGTERM），这层关系此前只写在 main.cpp 的注释里，现在落到目录结构上。
  头文件顺带写清它管的是两件事（信号 + Qt 消息重定向）、以及 `exceptionCaught`
  信号目前没有消费者（留作扩展点）。移到了新目录，但 `挂起事项.md` 里那条
  "SIGTERM 杀不掉应用"的缺陷仍在，只是线索路径跟着改了
- **删掉从未被使用的 googletest 1.10.0 与 crashpad 自带的测试封装**：
  `cmake/crashpad/crashpad.cmake` 只 include 了 9 个库模块，`gtest.cmake`、
  9 个 `crashpad-*-test.cmake`、`CTestCustom.cmake`（合计约 1200 行）以及
  `crashpad-mig.cmake`（macOS 的 `mig` 工具封装，`find_package(Python2)`）
  从未参与构建——本仓不开发 crashpad，也不跑它的自测，留着只会让人以为它们
  在生效。随之删掉 `3rdparty/googletest-release-1.10.0.tar.gz` 与解压清单里
  的对应条目：它唯一的潜在消费者就是那些测试文件。本仓测试继续用 1.17.0
  （`cmake/test_helpers.cmake` 与顶层 `add_subdirectory` 本就在用它）。
  为什么不是反过来"将就 crashpad 用 1.10"：1.10.0 的
  `cmake_minimum_required(2.8.8)` 在 CMake 移除 <3.5 兼容后是硬错误（WSL 的
  3.31 必踩），为它降级还得再打一个补丁，而收益为零
- **第三方改动一律走 patch 文件，不再用 CMake 文本替换**：SARibbon 与
  QWindowKit 一共 5 处改动（Qt5 锁定、`cmake_minimum_required` 抬到 3.15、
  QWindowKit 改走 in-tree 目标、去掉 `install(EXPORT)`、qmsetup 源码路径），
  全部落在 `patches/SARibbon/` 与 `patches/QWindowKit/`，由
  `patch_dependencies.cmake` 在 configure 时应用——可 review、可重放，重解压
  后自动重放。补丁由 pristine 解压副本逐条 `git diff` 生成，`--ignore-whitespace`
  解决 CRLF 目标的匹配问题（见下）
- **`patch_dependencies.cmake` 给 `git apply` 加 `--ignore-whitespace`**：
  SARibbon 的 `CMakeLists.txt` 是 CRLF，而本仓 `.gitattributes` 把 `*.patch`
  钉成 LF，行尾不一致会让 `git apply` 直接报 `patch does not apply`（实测）。
  代价是补丁新增/修改的那几行按 LF 落盘，目标文件成为**混合行尾**——CMake
  两者都认，且该文件是解压产物、每次解压重新生成，故接受
- **顶层显式 `set(QT_VERSION_MAJOR 5)`**：qmsetup 的 `qm_find_qt` 只在
  `QT_VERSION_MAJOR` 未定义时才做版本无关查找，而那个查找走的是包名 `QT`、
  认 `QT_DIR`，与本仓 `local.cmake` 只提供 `Qt5_DIR` 的写法对不上，配置会直接
  失败。钉死后它转为 `find_package(Qt5 ...)`，复用已解析好的 `Qt5_DIR`
- **funq 示例脚本里的设置项路径变短**：`MainWindow::QMenuBar::menuTool::
  actionSettings` → `MainWindow::actionSettings`。动作都 `new` 在主窗口上，
  而 SARibbon 的 `addLargeAction` 不改其父级，所以前缀就是窗口自己；引导导览的
  目标名也随形态切换（功能区指面板 `ribbonPanelTool` / `ribbonPanelHelp`，
  菜单栏形态仍是 `menuTool` / `menuHelp`）
- **`main.cpp` 骨架化**：它原来是一篇 200 行的说明文——12 件事平铺在一个 `main()`
  里，顺序约束散在五处注释中，读者拼不出全局。现在顶部有三块说明（启动时序图、
  对象所有权约定、"标了「可删」的段落可整段拿掉"），正文分成 8 段，Splash 那段
  只负责报进度、干活的抽成具名函数（`InitializeSession` / `LoadWindowSize`），
  UI 测试接线收进 `SetupUiTesting`（`#ifdef` 封在函数里）。基础设施那几步**刻意
  没抽**——`CrashHandler::create()` 返回 `unique_ptr`，抽进辅助函数会在返回时析构、
  顺带撤掉崩溃捕获。命令行解析换成 `QCommandLineParser`：`--help` / `--version`
  白送，未知参数直接报错（以前是被静默忽略）
- **引导演示层不再靠 Qt 父子关系定位**：原先把遮罩 `setParent` 到 viewport——模态
  对话框一开就盖不到它上面（子控件永远压不过同为顶层窗口的模态框），而换宿主时
  宿主被销毁又会把演示层一起带走。改成独立顶层窗口（`Qt::Tool` + 无边框 + 半透明）
  + `SetHost()` 手动 `setGeometry`/`raise` 管理层级。连带一处：实操模式下气泡也得
  升成自己的顶层窗口（遮罩不再是某个窗口的子控件，挂回宿主会被压在下面），并补了
  坐标原点偏移
- **单实例成为铁律，回放不再豁免**：UI 测试回放此前有意跳过单实例检测，理由是
  怕被当成第二实例转发参数后以 0 退出、ctest 看到"通过"而事件一个没跑。但那样
  会让回放**真的起第二个实例**，两个实例抢同一份日志与配置——日志那份已经炸过
  一次。现改为：检测到已有实例时**明确失败**（退出码 2，与"回放失败"的 1 分开；
  stderr 说明原因），既不假过也不起第二个
- **注入启动前先探一下被测程序在不在跑**（`examples/funq/check_no_instance.py`）：
  funq 的 runner 是「先起进程、再注入」，而已有实例时新起的进程会转发参数后
  立刻退出——Windows 侧 runner 睡 1 秒才抛一句看不出原因的
  `The process has finished with an error ...`，Linux 侧连存活检查都没有、静默
  返回。探测复用被测程序自己的单实例端点（Windows 命名管道 / Linux unix
  socket），不按进程名扫；它只是提前告知，程序自己的检测仍是权威，探不到就
  照常启动
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
- **崩溃排查与发布指南补上 Linux 侧**：`docs/guide/构建配置与崩溃排查.md`
  此前通篇是 Windows（PDB、minidump、WinDbg），而 Linux 的捕获机制、日志格式
  与符号载体都不同；`docs/guide/发布指南.md` 同样只有 `.bat` + ISCC 一条路。
  现两份都按平台并列改写，并补上在 WSL 实测出来的结论：Linux 崩溃日志名是
  `crash_<Unix 时间戳>.log`（与 Windows 的日期格式不同）、退出码是
  `128+信号号`、解析时必须取圆括号里的 `+0x…` 偏移（PIE 的绝对地址喂给
  `addr2line` 只得到 `??`）；`release` 产物给出函数名但无行号，而 AppImage 里
  strip 过的那份会给出**错误的函数名**——符号表没了，`addr2line` 退回到最近的
  可疑符号。发布指南另记了产物命名的两处不一致：符号目录的架构用 `x64`
  （`APP_ARCH_SUFFIX`），AppImage 文件名的架构用 `x86_64`（`APPIMAGE_ARCH`）
- **新增《调试 libFunq》指南**：注入库没有哪个工程把它编进产物，按 F5 调不了，
  只能附加到进程；而注入时机又被 runner 的「起进程 → 睡 1 秒 → 注入」卡死，
  一个不留神就只能断在初始化之后。文档记下实测可行的分工——VS Code 在
  `runner_win.py` 的 `_inject_dll()` 上打「暂停键」，Visual Studio 附加被测程序
  并在库加载前挂好待绑定断点（空心转实心），另外记了 `--library` 必填与
  `command` 是 `REMAINDER`（选项要写在被测程序路径前）两处易错点

### 修复

- **Linux 侧 ctest 多报两条 `example`/`example64` 失败**（`patches/zlib/0002`）：
  crashpad 模式下 `cmake/crashpad/crashpad.cmake` 会把 `3rdparty/zlib-v1.2.11`
  以 `EXCLUDE_FROM_ALL` 加进来给 crashpad 供 `zlibstatic`，而 zlib 自己的
  `CMakeLists.txt` 给四个示例程序（`example`/`minigzip`/`example64`/`minigzip64`）
  无条件 `add_test` ——目标从不编译，测试却进了 ctest，于是报两条 `Not Run`，
  一轮跑下来是 67% passed。`builtin` 模式不 include crashpad.cmake，所以
  Windows 侧看不到。**关不掉，只能补丁**：`add_test` 没有开关；从别的目录作用域
  `set_tests_properties` 要写 `DIRECTORY`，那是 CMake 3.28 才有的选项，而本仓
  声明 `cmake_minimum_required(VERSION 3.19)`（`DISABLED` 属性本身 3.9 就有，
  卡住的是作用域——文档写明「If the test is not found, CMake will report an
  error」，测试的查找是按目录作用域走的）。补丁整段删掉「Example binaries」，
  实测 WSL 的 `ctest -N` 从 6 条回到 4 条，全集 100% passed
- **底座层的下拉框与微调框看不到箭头**：`base/input.qss` 里只给箭头子控件写了
  尺寸、没给 `image`——子控件一旦被规则接管，Qt 就不再画原生箭头，结果是一个
  都没有（离屏探针实测整个控件里最暗的像素是边框色 204，箭头区域零墨迹）。
  补上 `arrow_up.svg` / `arrow_down.svg`（16×8 紧画布、描边 1.6，与 QSS 尺寸
  1:1，不做二次缩放），同时删掉 `subcontrol-origin: margin; subcontrol-position:
  right center`——实测那写法算出的箭头矩形 y 为负，等于把箭头排在控件外。另外
  微调按钮原来写死 `height: 14px`，控件高 38px，两个按钮之间空了 10px，改成不写
  高度由 Qt 对半分；下拉框右侧内边距 36 → 34（下拉区 32 + 分隔线 1 + 1px 余量），
  文字区宽 81 → 83。改后用同一探针复测：箭头墨迹 12×5 且居中、缝 0
- **pick 模式的输出看不见**（`patches/funq/0008`）：pick 把 `WIDGET:` / `GITEM:`
  直接写 C stdout（`pick.cpp` 的 `QTextStream(stdout, ...)`），而本仓的日志文件
  与界面日志窗口都接在 Qt 消息处理器上——这两处永远不会有它。更麻烦的是 stdout
  本身也常常无处可去：被测程序是 GUI 程序，又被 runner 经 Python 的管道链启动，
  标准句柄无效，写入被静默丢弃（实测同一次点击：日志里有 `WIDGET:` 行，而重定向
  文件一个字节都没有；钩子、点击、路径解析其实全是好的）。现在 `handle()` 先把
  整块拼成 `QString`，照旧写 stdout（多行，终端里竖着读），再经 `qInfo()` 发一遍
  ——**一次点击是一条记录**，属性行保留自己的换行，于是前缀只出现一次。按行发会
  看着像 N 件不相干的事；光把整块丢给面板还不够，它的 HTML 渲染会把换行折叠掉，
  得给条目样式加 `white-space:pre-wrap`（`log_output_panel.cpp`）
- **日志面板每条消息后多两个空行**：`pre-wrap` 让换行不再被折叠，同时也暴露出
  一直藏在消息末尾的 spdlog 行尾——格式化器会给每条记录补 `SPDLOG_EOL`，Windows
  上是 `"\r\n"`，在 `pre-wrap` 下 `\r` 与 `\n` 各算一次换行，于是每条后面多两个
  空块（实测 `blockCount = 3`：消息 + 两个空块）。这行尾是给按行读的 sink 用的，
  Qt 侧每条消息自成一个文本块，`QtConsoleSink::sink_it_` 现在把它剥掉
- **日志窗口补上应用内关闭按钮**：UI 回放**录不到标题栏的 X**——标题栏属非客户区，
  关闭由系统处理、直接发 `QCloseEvent`，没有鼠标事件经过录制器的过滤器；而库里也
  没有任何 player 支持 `close` 命令，想在 XML 里手写一条"关闭"都写不出来。于是
  日志窗口在脚本里"只进不出"：录制时无声无息，回放时一直挂着。加了与设置对话框
  `settingsCloseButton` 同款的应用内关闭按钮。这条也写进了模板使用指南——凡会被
  回放打开的窗口，都得有应用内关闭入口
- **QtTesting 控制面板的图标全丢了**：`pqRecordEventsDialog` / `pqPlayBackEventsDialog`
  的按钮全靠在 `QtTesting.qrc` 里，而静态库的 qrc 符号没人引用就不会被链进来——
  面板上是一排**空白按钮**，其中「校验模式」那个更是**只有图标没有文字**，看着像
  没有功能。补上一次显式引用（`Q_INIT_RESOURCE(QtTesting)`）后图标回来
- **回放模式撞上已运行的实例会 abort**：`Logger::init()` 里滚动文件 sink 用
  `rotate_on_open=true` 构造，一建就轮转一次日志、要把 `<应用名>.log` 改名成
  `.1.log`。另一个实例正开着这个文件时 Windows 拒绝改名，spdlog 抛异常而无人
  接管，随即 `std::terminate` → `abort`——进程连一行日志都写不出来（退出码 3，
  并弹一个 CRT 错误框）。平时跑不出来，是因为第二个实例早在 `SingleInstance`
  那层就被挡回去了，而 UI 测试回放**有意**绕过那层（否则会被当成第二实例转发
  参数后以 0 退出，ctest 看到"通过"而事件一个没跑）。现在文件 sink 的构造包了
  降级：先退到带进程号的独立文件，再不行就不写文件，控制台与界面 sink 照常
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
- **注入的 funq 库可能与被测程序不同构建配置**：暂存目录在源码树里只有一个
  （`3rdparty/funq-master/server/funq_server/`），而后一次构建会覆盖前一次留下的
  库。于是"在 debug 目录跑测试、但上一次全量构建是 relwithdebinfo"就成了往
  Debug 程序里注入 Release 库——Qt 的 debug 二进制（`Qt5Cored`）与 release
  （`Qt5Core`）互不兼容，现场只有 `LoadLibraryW` 返回 0 或
  `WaitForSingleObject() returned 258`（附带一句误导人的"Maybe x86/x64 mismatch"）。
  现改为**按构建类型分名暂存**（`libFunq-debug.dll` / `libFunq-relwithdebinfo.dll`），
  并给 funq 的 runner 加 `--library`（`patches/funq/0007`），由 `funq_attach` 与
  `funq-server-demo` 两个包装脚本显式传入本构建目录对应的那一份；无后缀那份仍保留，
  供手工 `python -m funq_server.runner` 使用（沿用"最后一次构建说了算"的旧行为，
  文档已写明）
- **Linux 侧 Debug 打包被拒时，提示里给的是 Windows 的命令**：
  `check_package_config.cmake` 的 Linux 分支打印 `cmake --preset
  ninja-relwithdebinfo-x64 ...`，而 Linux 侧根本不用 preset（原因见
  `build_ninja.sh` 头部注释），照抄只会再撞一次错。改为
  `./scripts/build_ninja.sh -t relwithdebinfo -p`
- **Linux 侧打包可能执行到别的构建目录的脚本**：`scripts/make_appimage.sh`
  由 `configure_file` 生成在**源码树**里，同一棵树的多个构建目录共用这一个
  文件，后 configure 的会覆盖先前的。于是从旧构建目录里跑 `make_appimage`
  执行的其实是别人的脚本——实测报「未找到
  build/linux-ninja-release/bin/demo_app」，而更糟的是：若被覆盖的那份是
  relwithdebinfo 生成的，Debug 打包的拦截会一并被绕过去。现要求调用方传入
  可执行文件所在目录，与脚本里烧死的构建目录比对，不符即报错退出（退出码 2）。
  Windows 侧早有同款守卫（`make_installer.bat` 的 `EXPECTED_BIN` 比对），
  此次是把 Linux 侧补齐

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
