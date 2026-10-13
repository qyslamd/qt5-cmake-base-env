# 调试 libFunq

libFunq 是被**注入**到被测进程里的，仓库里没有任何一个工程把它的源码编进自己
的产物，所以「按 F5 调试」这条常路走不通。但库进到进程之后就是一个普通模块：
**附加到进程**即可调试，调试器不关心它是怎么进来的。

本文讲这条路。重点是让「注入」这个动作变成可控的一步——这正是 VS Code 那侧
一个 Python 断点能提供的：被测程序已经跑起来、库还没进去，此时有无限时间从容
附加，并且能在模块加载**之前**把 C++ 断点挂好。

## 前置条件

- **库与被测程序同一构建配置、同一位数**。Debug 的 Qt 二进制（`Qt5Cored.dll`）
  与 release（`Qt5Core.dll`）互不兼容，混用会让 `LoadLibrary` 直接失败，而错误
  信息毫无提示性。位数同理（32 位注不进 64 位进程）。
- **符号**：PDB 在构建目录里（`build/ninja-<类型>-x64/bin/Funq.pdb`），
  `relwithdebinfo` 也带 `/Zi`，同样可调。暂存到源码树的那份是**拷贝**，PE 里
  记的 PDB 路径仍是构建目录的绝对路径，那个目录还在就自动加载符号。只有挪走
  或删掉构建目录才需要手工加符号路径（工具 → 选项 → 调试 → 符号）。
- **看 Qt 类型要 natvis**：没有它，监视窗口里的 `QString` / `QList` 只是一坨
  `{d=...}`，调 funq 时（`ObjectPath::objectPath` 的结果、pick 打出来的属性）
  基本没法看。VS Code 侧不用管——Qt 扩展自带了；
  `theqtcompany.qt-cpp-<版本>/res/natvis/` 下有 `qt5.natvis` 与 `qt6.natvis`，
  前者由扩展生效。Visual Studio 侧要手动放一份，两个位置任选：全机的
  `<VS 安装目录>\Common7\Packages\Debugger\Visualizers\`（盘符跟着 VS 装在哪，
  往 Program Files 里写要管理员权限），或单用户的
  `%USERPROFILE%\Documents\Visual Studio <版本>\Visualizers\`（免管理员）。
  这条与 libFunq 无关，任何 Qt 程序在 VS 里调都这样，装一次即可。

## 流程

分两半：VS Code 管 Python 侧的 runner，Visual Studio 管 C++ 侧的注入库。

1. **VS Code 打「暂停键」**：在 `3rdparty/funq-master/server/funq_server/`
   `runner_win.py` 的 `_inject_dll()` 开头下一个断点。`start_subprocess()` 是
   「起进程 → `time.sleep(1.0)` → 检查进程存活 → `_inject_dll()`」，断在这里
   正好是被测程序已启动、库还没注入的时刻。
2. **VS Code 起 runner**：运行配置选 `Python Debugger: Current File with
   Arguments`（它的 `args` 是 `${command:pickArgs}`，F5 时会弹窗要参数），当前
   文件打开 `funq_server/runner.py`，弹出的参数框里给：

   ```text
   --library <仓库>\3rdparty\funq-master\server\funq_server\libFunq-debug.dll <仓库>\build\ninja-debug-x64\bin\demo_app.exe
   ```

   两个要点：
   - `--library` 不能省。不带它就用暂存目录里那份 `libFunq.dll`，那是**最后一次
     构建**的配置，与被测程序不匹配时失败得莫名其妙（见
     [funq 注入式 UI 测试](funq注入式UI测试.md)的构建类型一节）。
   - 位置参数 `command` 是 `argparse.REMAINDER`，选项必须写在被测程序路径
     **前面**，否则会被当成传给被测程序的参数。

   参数不必自己拼：`build/ninja-<配置>-x64/bin/funq-server-demo.bat` 里就是一条
   完整的 runner 命令行（`--library` 与被测程序路径都在），把
   `python -m funq_server.runner` 之后的参数照抄进弹窗即可。注意脚本里那份库与
   程序是**它自己配置**的，换配置要一并换。

   需要 pick 模式再加 `--pick`（它同样写在前面）。该运行配置的 `env` 里已经备好
   PATH（Qt 的 `bin`）与 PYTHONPATH（funq 的 client / server 目录），被测程序
   继承这个环境才找得到 Qt。

3. **Visual Studio 附加**：调试 → 附加到进程 → 选 `demo_app.exe` → 附加到
   「本机代码」。被测程序是 GUI 程序，进程列表里按名字找。用
   调试 → 窗口 → 模块 可确认库是否已加载、符号是否已加载。不想另开 Visual
   Studio 的话，留在 VS Code 用 `.vscode/launch.json` 里的 `(Windows) Attach`
   配置也行（`cppvsdbg` 就是 MSVC 的调试器，待绑定断点的行为一致）。
4. **挂 C++ 断点**：文件 → 打开 → 文件夹，选
   `3rdparty/funq-master/server/libFunq`，打开要断的文件打断点。此刻库还没加载，
   断点是**空心**的——这是待绑定断点，模块一进来就会绑上。
5. **放行**：回到 VS Code 按继续。注入执行，库被 `LoadLibrary` 载入，Visual
   Studio 那边的断点转**实心**，之后 `funqInit()`、`pick.cpp` 的钩子、
   `player.cpp` 的服务都能正常调试。库加载前挂的断点全都绑得上，所以 `DllMain`
   与静态初始化也能断——这是这条流程压过「事后附加」的地方。注意 `DllMain`
   里持有**加载器锁**：别单步太久，也别让它去调 `LoadLibrary`、创建线程之类，
   看一眼就放行。

## 已经注入之后再附加

只是断不进初始化路径（`DllMain`、等 `QCoreApplication` 就绪的那段自旋已经跑完），
其余一样：附加、打开源码文件夹、打断点，库里的钩子与服务都还在跑。省掉 VS Code
那两步即可。

## 不需要控制注入时机时

只想调运行期（`player.cpp` 的 TCP 服务、`pick.cpp` 的钩子），不在乎断不进
`DllMain` 与静态初始化，就不必让 runner 起进程：自己在调试器里 F5 起 demo_app
（`.vscode/launch.json` 里已有 `demo_app (x64 Debug)` / `(x64 RelWithDebInfo)`
两条配置，注意选哪条就注入哪个配置的库），再用 `funq_server.runner_win` 里
`WindowsRunnerInjector` 的 `_inject_dll(pid, library)` 补一次注入——Windows 侧的
注入本来就是「进程跑起来之后再注入」，runner 与这几行 Python 干的是同一件事。
限制有两条：一是够不着库的初始化路径；二是它只能吃**目标进程自己环境里**的
变量，`FUNQ_MODE_PICK`、`FUNQ_PORT`、`FUNQ_HOST` 都是库在自己进程内读的，runner
「启动前塞进子进程环境」这一手补注入没有，所以 pick 模式与非默认端口都用不了，
必须在启动 app 时就把变量给好。

## 几个坑

- **改在 `3rdparty/` 里的源码会被冲掉**：那是解压产物，删掉重建时会重新解压再
  打补丁。调试时临时改两行没问题，想留住就得做成补丁
  （`patches/funq/NNNN-*.patch`，在 `cmake/patch_dependencies.cmake` 注册）。
- **注入的是暂存的那份**：
  `3rdparty/funq-master/server/funq_server/libFunq-<架构>-<配置>.dll`
  （如 `libFunq-x86-relwithdebinfo.dll`），由构建时的 `stage_funq_injector` 从
  `build/ninja-<配置>-x64/bin/Funq.dll` 拷来。改完 funq 源码要重新构建；
  **只编 `Funq` 目标不会更新暂存**——暂存挂在 `ALL` 目标上。
- **pick 模式的断点会冻住界面**：钩子在 GUI 线程上，断在 `PickFormatter::handle()`
  时窗口不响应，属正常现象。
- **附加需要权限**：被测程序若以管理员身份运行，Visual Studio 也要以管理员身份
  启动，否则附加会被拒绝。
- **调试结束选「分离」**：选「停止调试」会结束 demo_app 进程，Python 侧的 runner
  随之报错退出。

## 相关文档

- 注入机制本身、pick 模式怎么用、控件路径怎么查：
  [funq 注入式 UI 测试](funq注入式UI测试.md)
- 拿不到断点时看日志与崩溃转储：
  [构建配置与崩溃排查](构建配置与崩溃排查.md)
