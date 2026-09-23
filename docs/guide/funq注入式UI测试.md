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
[examples/funq](../../examples/funq/)——两个终端各起一边，被测程序是**本仓自己的
demo_app**，七步从登录一路驱动到设置对话框，每步都打印它看到了什么。

## 前置条件

**Python 3.8 或更高版本**，仅测试需要，**编译不需要**。

不需要 `pip install` 任何东西：funq 的客户端与服务端源码就在
`3rdparty/funq-master/{client,server}` 里，由包装脚本挂到 `sys.path` 上。上游
`setup.py` 声明的唯一依赖 `nose` 用不到——它只被 funq 的 nose 插件引用，而那个
插件已由 `patches/funq/0004-drop-nose-plugin.patch` 删除（nose 自身在
Python 3.10+ 也已无法运行：依赖被移除的 `imp` 模块）。

## 跑测试

```bat
:: Windows：只跑 funq 用例
ctest --test-dir build\ninja-debug-x64 -R test_gallery_funq --output-on-failure
```

```bash
# Linux / WSL（构建见 scripts/build_ninja.sh）
ctest --test-dir build/linux-ninja-debug -R test_gallery_funq --output-on-failure
```

去掉 `-R` 就是全套（含单元测试与 QtTesting 回放）。

每个用例都要拉起一次程序并等注入完成。Windows 侧一轮约 10 秒（funq 在启动后
固定 `sleep 1` 秒才注入，见 `funq_server/runner_win.py` 的注释）；Linux 侧走
`LD_PRELOAD`，没有这段固定等待，实测一轮约 5 秒。若某一轮明显变长，多半是有
用例卡在连接超时上，不是正常耗时。

## 机制

```text
FunqTestCase（用例）
   │  按 funq.conf 拉起被测程序：cmd = [funq_attach] + [libui_gallery]
   ▼
funq_attach.bat / funq_attach.sh → python -m funq_server.runner
   │  Windows：启动程序 → sleep 1s → 检查进程存活
   │           → CreateRemoteThread + LoadLibrary（runner_win.py）
   │  Linux  ：把 libFunq.so 塞进 LD_PRELOAD 再 exec（runner_linux.py）
   ▼
libFunq.dll / libFunq.so（注入进 gallery 进程，在里面起 JSON 服务）
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

## 写用例

### 控件路径必须从运行中的进程里查

被测的 gallery 示例没有给控件设 `objectName`，funq 的路径由「类名 + 同名兄弟的
序号后缀」拼成（`objectpath.cpp` 的 `rawObjectName`），而序号取决于子对象的创建
顺序；布局与分裂器还会改变父子关系——源码里写 `new QTabWidget(this)`，实际被
`addWidget(splitter)` 挪到了 `QSplitter` 下。**所以路径不能读源码推**。

```bat
:: Windows，从仓库根目录执行
set D=build\ninja-debug-x64\tests\gallery_funq
set FUNQ_CONF=%CD%\%D%\funq.conf
set FUNQ_ATTACH_EXE=%CD%\%D%\funq_attach.bat
set FUNQ_CLIENT_DIR=%CD%\3rdparty\funq-master\client
set FUNQ_SERVER_DIR=%CD%\3rdparty\funq-master\server
python tests\gallery_funq\probe_widgets.py SwitchButton
```

```bash
# Linux / WSL，同样从仓库根目录执行
D=build/linux-ninja-debug/tests/gallery_funq
FUNQ_CONF=$PWD/$D/funq.conf \
FUNQ_ATTACH_EXE=$PWD/$D/funq_attach.sh \
FUNQ_CLIENT_DIR=$PWD/3rdparty/funq-master/client \
FUNQ_SERVER_DIR=$PWD/3rdparty/funq-master/server \
python3 tests/gallery_funq/probe_widgets.py SwitchButton
```

四个环境变量都要给：探针靠 `FUNQ_CONF` / `FUNQ_ATTACH_EXE` 拉起程序并注入，靠
后两个把 funq 的包挂到 `sys.path` 上（探针不走 `run_funq_tests.py` 那条路，
没有别的地方替它加）。`ctest` 跑用例时这几项由 CMake 在 `add_test` 里注入，
手工调用才要自己设。

探针会拉起程序、注入、把路径含该关键字的控件全部打印出来。改完界面后重新跑一遍，
把新路径更新到 `gallery.alias`。

**只想确认某一个控件时，用 pick 模式更直接**——它不开 TCP 服务，而是在整个
应用上装事件钩子（`libFunq/pick.cpp`），**按住 Ctrl+Shift 点任何控件**，它的
funq 路径连同非空属性就打到 server 终端的 stdout 上：

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

- **「模式生效」的确认不在终端**：那句 `Funq mode PICK in action !` 是
  `qDebug` 打的，而本仓的被测程序都装了 Qt 消息重定向（demo_app 收进 spdlog
  日志、gallery 收进界面日志面板），它不会出现在控制台——去应用日志里找
  （demo_app 在 `%LOCALAPPDATA%/<组织>/<应用>/logs/`）。终端里真正会出现的
  是 Ctrl+Shift 点控件后的 `WIDGET: ...` 行：那走 C stdout，不经消息重定向
- pick 与正常模式**互斥**（`funq.cpp` 的 `funqInit()`：PLAYER 开 TCP 服务，
  PICK 装事件钩子，二选一）——pick 下客户端连不上，它是个找路径的工具，
  不是「边连客户端边挑」
- pick 输出在 **server 那个终端**，别去客户端找

### 用别名让用例不怕路径变化

`tests/gallery_funq/gallery.alias` 是 funq 的别名表（`友好名 = 路径`），用例里可以
写 `self.funq.widget('switch_button')`。路径变了只改这一处。

### 两个坑

- **不可见的控件要 `wait_active=False`**：`widget()` 默认会等控件 `visible` 且
  `enabled`，非当前标签页的控件不满足，会一直等到超时。取它们时要显式关掉。
- **用完把状态复位**：用例改过的属性（标签页索引、下拉框选项等）要在 `finally`
  里设回去，否则后续用例看到的是被改过的界面。

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
taskkill /F /IM libui_gallery.exe
```

```bash
pkill -f libui_gallery
```

Linux 侧走不到「库被占用删不掉」那一段——允许删除正在使用的文件——但残留进程
一样占着显示连接与端口，仍要清掉。

**构建类型与位数必须一致。** Debug 的 `libFunq.dll` 链 `Qt5Cored.dll`，注入 Debug 的
gallery 才有同一套 Qt 运行时；拿 Release 的库注入 Debug 的程序会在一个进程里加载
两套 Qt。位数同理（`runner_win.py` 走 `CreateRemoteThread`，32 位注不进 64 位进程）。

**需要桌面环境。** 测试要真实窗口与事件循环，纯 CI 容器里跑不了；WSL 下靠 WSLg。

## 平台差异

| | Windows | Linux / WSL |
|---|---|---|
| 包装脚本 | `funq_attach.bat` | `funq_attach.sh`（生成后补 `+x` 位） |
| 注入方式 | 启动程序 → `sleep 1` → `CreateRemoteThread` + `LoadLibrary` | 进程启动前设 `LD_PRELOAD`，随构造器一起加载 |
| 注入库 | `libFunq.dll` | `libFunq.so` |
| 注入时机 | 被测程序已在运行 | 构造器跑在 `main()` 之前，库内线程自旋等 `QCoreApplication` 就绪 |

两侧的包装脚本都由 CMake 生成到构建目录，模板分别是
`tests/gallery_funq/funq_attach.bat.in` 与 `funq_attach.sh.in`。

**全新编出来的第一次运行可能卡在连接超时。** 实测出现过一次：刚编完的二进制
第一次被拉起时，funq 客户端 10 秒的连接预算（`client.py` 的 `wait_for`）没等到
服务端起来，抛了 `ConnectionRefusedError`，之后每轮都过。怀疑是首次加载要摊开
Debug 构建的可执行文件与几十兆 Qt 库（冷页缓存），但没有确证。要留余量可在
`funq.conf` 里加 `timeout_connection = 30`——`client.py` 支持这一项，默认 10 秒；
注意它同时是后续每次 socket 操作的超时，放宽意味着真卡住时要多等。
