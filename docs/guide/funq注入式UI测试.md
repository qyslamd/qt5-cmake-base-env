# funq 注入式 UI 测试

用 Python 驱动真实 Qt 程序的界面回归测试：测试时把 `libFunq` 注入被测进程，
用例按控件路径操作界面并断言属性。被测程序**不需要为测试改一行代码**。

与本仓另一套 UI 测试（QtTesting，见[模板使用指南](模板使用指南.md)的「UI 测试」
一节）的分工：

| | QtTesting | funq |
|---|---|---|
| 侵入性 | 被测程序要带测试菜单（编译期开关） | 零侵入，注入到运行中的进程 |
| 用例形式 | 在程序里录一遍操作，存成 XML 再回放 | 用 Python 写，可断言任意属性 |
| 适合 | 手工录一次、回归回放 | 需要判断"界面状态对不对"的用例 |

本文讲的是**自动化**那条链路：一条 ctest 命令跑完，被测程序由用例自己拉起。
想先看清 funq 的 server 侧与 client 侧是怎么分工的，看
[examples/funq-scripts](../../examples/funq-scripts/)——两个终端各起一边，
被测程序是**本仓自己的 demo_app**，七步从登录一路驱动到设置对话框，每步都打印
它看到了什么。

## 前置条件

**Python 3.8 或更高版本**，仅测试需要，**编译不需要**。Windows 上它的**位数必须
与目标架构一致**（32 位构建配 32 位 Python），否则这条链路跑不起来——原因与表现
见下面「排错」里那条。

不需要 `pip install` 任何东西：funq 的客户端与服务端源码就在
`3rdparty/funq-master/{client,server}` 里，由包装脚本挂到 `sys.path` 上。上游
`setup.py` 声明的唯一依赖 `nose` 用不到——它只被 funq 的 nose 插件引用，而那个
插件已由 `patches/funq/0004-drop-nose-plugin.patch` 删除（nose 自身在
Python 3.10+ 也已无法运行：依赖被移除的 `imp` 模块）。

## 跑测试

靶子是 `examples/funq_target`：一个三页签的小窗口，输入页（文本框 / 下拉框 /
微调框 / 复选框）、显示页（标签 / 进度条 / 列表）、动作页（两个按钮 + 计数
标签），控件之间有几条真实的联动——改文本框的文本，显示页的标签跟着变。

```bat
:: Windows：只跑 funq 用例
ctest --test-dir build\ninja-debug-x64 -R test_funq_demo --output-on-failure
```

```bash
# Linux / WSL（构建见 scripts/build_ninja.sh）
ctest --test-dir build/linux-ninja-debug -R test_funq_demo --output-on-failure
```

去掉 `-R` 就是全套（含单元测试与 QtTesting 回放）。

每个用例都要拉起一次程序并等注入完成。Windows 侧一轮约 10 秒（funq 在启动后
固定 `sleep 1` 秒才注入，见 `funq_server/runner_win.py` 的注释）；Linux 侧走
`LD_PRELOAD`，没有这段固定等待，实测一轮约 5 秒。若某一轮明显变长，多半是有
用例卡在连接超时上，不是正常耗时。

## 机制

```text
FunqTestCase（用例）
   │  按 funq.conf 拉起靶子：cmd = [funq_attach] + [funq_target]
   ▼
funq_attach.bat / funq_attach.sh → python -m funq_server.runner
   │  Windows：启动程序 → sleep 1s → 检查进程存活
   │           → CreateRemoteThread + LoadLibrary（runner_win.py）
   │  Linux  ：把 libFunq.so 塞进 LD_PRELOAD 再 exec（runner_linux.py）
   ▼
libFunq.dll / libFunq.so（注入进靶子进程，在里面起 JSON 服务）
   ▲
   │  TCP：按控件路径操作界面、读属性
   └─ Python 客户端
```

`runner.py` 按平台自己分派到 `runner_win.py` / `runner_linux.py`，所以这一层
没有平台差异；有差异的是包装脚本（`.bat` / `.sh`）与注入库的放置。

三处看起来绕但都有原因的设计：

- **`funq_attach` 包装脚本**：funq 客户端构造的命令是 `[attach_exe] + [被测程序]`
  （`funq/client.py` 的 `_start_test_process`），所以 `attach_exe` 必须是「吃下后续
  参数再转交 funq runner」的可执行文件；直接拿 `python.exe` / `python3` 当它是错的，
  那会把被测程序当成脚本名。Linux 侧还有个额外要求：客户端是拿它当可执行文件
  `Popen` 的，所以 CMake 生成 `.sh` 后要补 `+x` 位。
- **注入库被拷到 `funq_server/` 目录并改名**：runner 的 `_find_library` 只认那一个
  位置、那一个名字（Windows 是 `libFunq.dll`，Linux 是 `libFunq.so`），而 MSVC 产出
  的是 `Funq.dll`。拷贝由顶层 `CMakeLists.txt` 的 `stage_funq_injector` 目标完成
  （用 `ALL` 目标而非 `POST_BUILD`：解压产物被删除重建时对象文件未必重编，
  `POST_BUILD` 不会再触发，库就悄悄消失了）。
- **注入库必须与被测程序同一构建配置**，所以暂存时按配置分名
  （`libFunq-debug.dll` / `libFunq-relwithdebinfo.dll` / …），由 `funq_attach`
  与 `funq-server-demo` 通过 `--library` 显式指定。不分名的后果很贵：暂存位置在
  源码树里只有一个，后一次构建会把前一次覆盖掉，于是「在 debug 目录跑测试，
  但上一次全量构建是 relwithdebinfo」就变成往 Debug 程序里注入 Release 库——
  Qt 的 debug 二进制（`Qt5Cored`）与 release（`Qt5Core`）互不兼容，表现为
  `LoadLibraryW` 返回 0，或走 runner 时超时并报一句
  `WaitForSingleObject() returned 258`（既有 x86/x64 混用的提示，与真实原因无关）。
  无后缀的那份仍然保留，供手工 `python -m funq_server.runner` 使用——那条路
  没有人告诉它该用哪份，只能沿用"最后一次构建"的旧行为
- **`funq_bootstrap.py` 手工注册应用配置**：funq 原本由它的 nose 插件负责读
  `funq.conf` 并把配置注册给测试基类，nose 不可用后由该脚本用 funq 自带的公开接口
  （`ApplicationRegistry.register_from_conf` + `register_funq_app_registry`）补上。

调试注入库本身（没有可 F5 的工程，要附加到进程）的步骤见
[调试 libFunq](调试libFunq.md)。

## 写用例

### 控件路径优先来自 objectName

靶子（`examples/funq_target`）里每个被测控件都显式设了 `objectName`，而 funq 的
objectpath 优先用 objectName，于是路径就是 `窗口objectName::控件objectName`——
稳定、可读，改界面不必重探。页签内的控件多一层
`qt_tabwidget_stackedwidget`，那是 Qt 给 `QTabWidget` 内部堆叠窗口起的
objectName，不是本仓的命名，省不掉。

**没设 `objectName` 的控件则回退**到「类名 + 同名兄弟的序号后缀」
（`objectpath.cpp` 的 `rawObjectName`），而序号取决于子对象的创建顺序；布局与
分裂器还会改变父子关系——源码里写 `new QTabWidget(this)`，实际被
`addWidget(splitter)` 挪到了 `QSplitter` 下。**那种路径不能读源码推**，得用
探针从运行中的进程里查。所以往靶子里加控件时，记得顺手设上 objectName。

```bat
:: Windows，从仓库根目录执行
set D=build\ninja-debug-x64\tests\funq_demo
set FUNQ_CONF=%CD%\%D%\funq.conf
set FUNQ_ATTACH_EXE=%CD%\%D%\funq_attach.bat
set FUNQ_CLIENT_DIR=%CD%\3rdparty\funq-master\client
set FUNQ_SERVER_DIR=%CD%\3rdparty\funq-master\server
python tests\funq_demo\probe_widgets.py QComboBox
```

```bash
# Linux / WSL，同样从仓库根目录执行
D=build/linux-ninja-debug/tests/funq_demo
FUNQ_CONF=$PWD/$D/funq.conf \
FUNQ_ATTACH_EXE=$PWD/$D/funq_attach.sh \
FUNQ_CLIENT_DIR=$PWD/3rdparty/funq-master/client \
FUNQ_SERVER_DIR=$PWD/3rdparty/funq-master/server \
python3 tests/funq_demo/probe_widgets.py QComboBox
```

四个环境变量都要给：探针靠 `FUNQ_CONF` / `FUNQ_ATTACH_EXE` 拉起程序并注入，靠
后两个把 funq 的包挂到 `sys.path` 上（探针不走 `run_funq_tests.py` 那条路，
没有别的地方替它加）。`ctest` 跑用例时这几项由 CMake 在 `add_test` 里注入，
手工调用才要自己设。

探针会拉起靶子、注入、把路径含该关键字的控件全部打印出来。拿到路径后更新到
`funq_target.alias`，或在用例里直接写。

**只想确认某一个控件时，用 pick 模式更直接**——它不开 TCP 服务，而是在整个
应用上装事件钩子（`libFunq/pick.cpp`），**按住 Ctrl+Shift 点任何控件**，它的
funq 路径连同非空属性就写出去（stdout 与应用日志两处，见下）：

```text
WIDGET: `MainWindow::settingsDialog::settingsCloseButton` (pos: 84, 201)
        enabled: true
        text: "关闭"
        visible: true
```

开启方式是给 runner 加 `--pick`（经环境变量 `FUNQ_MODE_PICK=1` 传给 libFunq）。
生成的 `funq-server-demo.bat` 不透传参数——跟在后面的参数会传给被测程序——
最省事的做法是复制一份再改（生成的脚本不入库，configure 会重新生成，改它没有
副作用）：

```bat
cd build\ninja-debug-x64\bin
copy funq-server-demo.bat funq-pick.bat
:: 编辑 funq-pick.bat，给 python -m funq_server.runner 那行加上 --pick
funq-pick.bat
```

跑起来后终端会打一句 `Funq mode PICK in action !`，确认模式生效。要挑别的
程序，照脚本里那行 runner 命令自己写一条即可。`--pick` 本身只是个语法糖：
runner 把它翻译成环境变量 `FUNQ_MODE_PICK=1` 塞进被测进程，自己在 shell 里
设置这个变量再启动，效果相同——变量只要能到达被测进程就行。三点注意：

- **输出同时进应用日志**（`patches/funq/0008` 起）：`WIDGET:` / `GITEM:` 那一块
  既照旧写 stdout（终端里竖着读），也经 `qInfo()` 发一遍——**一次点击是一条记录**，
  属性行保留自己的换行，所以日志里长这样（前缀只出现一次）：

  ```text
  [info] [default] WIDGET: `loginDialog::loginContent::loginForm::loginButton` (pos: 36, 31)
  	objectName: loginButton
  	enabled: true
  	text: "登录"
  ```

  于是被测程序的界面日志窗口与日志文件（`%LOCALAPPDATA%/<组织>/<应用>/logs/`）
  里**都能看到**（装了 Qt 消息重定向的才有界面日志面板，demo_app 有）。要看路径
  就打开日志窗口点，最省事
  （日志窗口的条目样式带 `white-space:pre-wrap`，否则富文本会把这块压成一行——
  见 `log_output_panel.cpp` 的 `buildEntryHtml`）
- **别指望 stdout 一定有人接**：被测程序是 GUI 程序，若它是被 Python 经管道链
  起的（本仓的 runner 正是如此），它的标准句柄是无效的，stdout 的写入会被
  静默丢弃——实测同一次点击，日志里有 `WIDGET:` 行而重定向文件里一个字节都
  没有。这也是加 0008 的原因
- **「模式生效」的确认要看日志**：那句 `Funq mode PICK in action !` 是 `qDebug`
  打的，而本仓的被测程序都装了 Qt 消息重定向，它只会进应用日志，不在控制台
- pick 与正常模式**互斥**（`funq.cpp` 的 `funqInit()`：PLAYER 开 TCP 服务，
  PICK 装事件钩子，二选一）——pick 下客户端连不上，它是个找路径的工具，
  不是「边连客户端边挑」

### 用别名让用例不怕路径变化

`tests/funq_demo/funq_target.alias` 是 funq 的别名表（`友好名 = 路径`），用例里可以
写 `self.funq.widget('submit_button')` 而不必铺开一长串路径。靶子的控件都带
objectName，路径本身已经稳定，这份表存在的意义只是缩短写法。

### 两个坑

- **不可见的控件要 `wait_active=False`**：`widget()` 默认会等控件 `visible` 且
  `enabled`，非当前标签页的控件不满足，会一直等到超时。取它们时要显式关掉。
- **用完把状态复位**：用例改过的属性（标签页索引、下拉框选项等）要在 `finally`
  里设回去，否则后续用例看到的是被改过的界面。

## 让 AI Agent 看见界面：按 oid 抓单个控件

Agent 看不到 GUI。日志能回答"发生了什么"，属性查询能回答"某个控件的状态是什么"，
但**回答不了"长什么样"**——样式表有没有生效、主题颜色对不对、布局挤没挤，只有像素
能回答。实测过一个例子：样式表属性读出来是好的（主窗口 `styleSheet` 7056 字符），
而屏幕上功能区就是没主题——**属性说"设了"，像素才说"生效了"**。

`client.take_screenshot()` **抓的是整屏**（服务端 `grab` 不带 `oid` 的分支），窗口
被别的窗口压着、或者不在最前，拍到的就不是它。真正有用的是**按 oid 抓单个控件**：
服务端支持 `oid` 参数（`Player::grab`，Qt5 走 `QPixmap::grabWidget`），客户端没有
包这个接口，直接发命令即可：

```python
import base64

from funq.client import FunqClient

client = FunqClient(host="localhost", port=9000)          # 服务端见「跑测试」那套
win = client.widget(path="MainWindow", timeout=30.0)      # 模型上的 .oid 就是对象 id
data = client.send_command("grab", oid=win.oid, format="PNG")
with open("shot.png", "wb") as fh:
    fh.write(base64.standard_b64decode(data["data"]))
```

几条实测要点：

- 抓的是**该控件自身渲染的位图，与窗口层级无关**——被压住也拍得到。这正是它比整屏
  截图有用的地方
- 只拍这棵子树。**独立顶层窗口不在其中**：对话框、引导遮罩、被 reparent 成顶层的气泡，
  要各自按路径抓
- 前置与其它 funq 客户端一样：被测程序已注入并在跑（`funq-server-demo.bat`），客户端
  侧 `PYTHONPATH` 挂 `3rdparty/funq-master/client`
- 同目录的 `tests/funq_app/probe_widgets.py` 是互补的另一个探针：那边列路径、查属性，
  这边看像素

## 排错

**残留进程会连锁出怪问题。** 用例启动失败时 funq 的清理路径会抛
`OSError: [WinError 6] 句柄无效`，被测程序留在后台；而它映射着 `libFunq.dll`，
于是：

```text
进程残留 → libFunq.dll 被占用 → 删不掉 3rdparty/funq-master/
        → 下次 configure 见目录存在就跳过解压 → 补丁打在残缺的树上
        → 报一堆「xxx: No such file or directory」
```

处理：

```bat
taskkill /F /IM funq_target.exe
```

```bash
pkill -x funq_target
```

**用 `-x` 而不是 `-f`**：`-f` 匹配的是整条命令行，而执行者自己那条
`pkill -f funq_target` 里也含这个字符串——实测在 WSL 里把自己那个 shell 一起
杀了，症状是命令跑一半终端就断。

Linux 侧走不到「库被占用删不掉」那一段——允许删除正在使用的文件——但残留进程
一样占着显示连接与端口，仍要清掉。

**构建类型必须一致。** Debug 的 `libFunq.dll` 链 `Qt5Cored.dll`，注入 Debug 的
靶子才有同一套 Qt 运行时；拿 Release 的库注入 Debug 的程序会在一个进程里加载
两套 Qt。

**暂存文件名分构建类型、但不分架构，两个架构会互相覆盖。** `stage_funq_injector`
把注入库拷进源码树的**共享**暂存目录，名字是 `libFunq-<构建类型>.<ext>`——x86 与
x64 的同名构建类型（都叫 relwithdebinfo）**撞名**。先编 x64 再编 x86，暂存的就是
64 位那份；x86 的 runner 去注入它就失败，报的却是：

```text
Failed to inject DLL! LoadLibraryW() returned c_ulong(0). The last error is 0x0
([WinError 0] 操作成功完成)。Maybe x86/x64 mismatch between python.exe and Qt DLLs?
```

（实测：先编 `build/ninja-relwithdebinfo-x64`，再跑 x86 的 `funq-server-demo.bat`。）
处理：**换架构前先按目标架构编一次**（重新暂存），或删掉暂存目录里那份让它重建。
判据是读 PE 头的 Machine（`0x14c` = x86、`0x8664` = x64）与靶子比一下。

**位数这条要对齐三方，而且错的一方不报错、只表现为超时。** 靶子、注入库、以及
**执注入的那个进程**——Windows 上是跑 `runner_win.py` 的 **Python**，它自己调
`CreateRemoteThread + LoadLibrary`，而跨位数注入是不允许的（64 位 Python 注不进
32 位靶子，反之亦然）。所以 Windows 的 x86 构建要用 32 位 Python：机器 PATH 上
那个通常是 64 位，`tests/funq_demo/CMakeLists.txt` 会在配置期让候选解释器自报一次
位数并与目标架构比对，不匹配就**跳过注册并提示**，用
`-DFUNQ_PYTHON=<32 位 python 的路径>` 指定一个匹配的即可。**踩过的样子**：x86 的
relwithdebinfo 构建里这条用例每个方法都 `ERROR`，然后挂到 ctest 的 180 秒超时——
看着像卡死，实际是注入从未成功。

用**嵌入式（embeddable）**分发包当这个 Python 时多一步：它自带 `pythonNN._pth`，
而这个文件一旦存在，解释器就进**隔离模式——`PYTHONPATH` 会被整个忽略**（`funq_attach`
脚本正是靠 PYTHONPATH 挂 funq 的两个包，所以到这儿就断了）。写 `import site` 也救不
回来：它只补 `<home>/site-packages`。**必须把 funq 的 client / server 两个目录直接
列进 `._pth`**。本仓自带的那两份就是 CMake 在解压后这么写的
（`cmake/extract_dependencies.cmake` 的 `configure_embedded_python_pth`），随安装包
分发的那份同理（见 `src/app/CMakeLists.txt` 的 `make_funq_kit`）。

**注入失败先查缺不缺 DLL，别急着怀疑位数。** `libFunQ.dll` 是在被测进程里被
`LoadLibrary` 载入的，它**有自己的依赖**，而 windeployqt 只按**主程序**的依赖
决定部署哪些 Qt DLL。本仓实测缺的是 `Qt5Test.dll`（funq 的库链了 `Qt5::Test`，
主程序在 `BUILD_QT_TESTING=OFF` 时不链），症状极具误导性：`LoadLibraryW` 返回
0，且 **last error 也是 0**，runner 的提示是
`Maybe x86/x64 mismatch between python.exe and Qt DLLs?`——看上去像位数问题，
实际是缺一个 DLL。源码树里跑测试撞不到这条：`add_test` 会把 Qt 的 bin 拼进
PATH，整个 bin 都在，注入库要什么有什么；只有按「只部署主程序依赖」的原则打
出来的包才会缺。随包分发的那份由构建期的 `cmake/collect_funq_qt_deps.cmake`
补齐（拷进 `funq\qt\`，启动脚本把该目录挂进 PATH），见[发布指南](发布指南.md)。

**需要桌面环境。** 测试要真实窗口与事件循环，纯 CI 容器里跑不了；WSL 下靠 WSLg。

## 平台差异

| | Windows | Linux / WSL |
|---|---|---|
| 包装脚本 | `funq_attach.bat` | `funq_attach.sh`（生成后补 `+x` 位） |
| 注入方式 | 启动程序 → `sleep 1` → `CreateRemoteThread` + `LoadLibrary` | 进程启动前设 `LD_PRELOAD`，随构造器一起加载 |
| 注入库 | `libFunq.dll` | `libFunq.so` |
| 注入时机 | 被测程序已在运行 | 构造器跑在 `main()` 之前，库内线程自旋等 `QCoreApplication` 就绪 |

两侧的包装脚本都由 CMake 生成到构建目录，模板分别是
`tests/funq_demo/funq_attach.bat.in` 与 `funq_attach.sh.in`。

**全新编出来的第一次运行可能卡在连接超时。** 实测出现过一次：刚编完的二进制
第一次被拉起时，funq 客户端 10 秒的连接预算（`client.py` 的 `wait_for`）没等到
服务端起来，抛了 `ConnectionRefusedError`，之后每轮都过。怀疑是首次加载要摊开
Debug 构建的可执行文件与几十兆 Qt 库（冷页缓存），但没有确证。要留余量可在
`funq.conf` 里加 `timeout_connection = 30`——`client.py` 支持这一项，默认 10 秒；
注意它同时是后续每次 socket 操作的超时，放宽意味着真卡住时要多等。
