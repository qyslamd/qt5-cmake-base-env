# funq 手动分离运行示例

用两个终端把 funq 的 server 侧与 client 侧**分开**跑，看清它的分工。被测程序
是本仓自己的 `demo_app`——「怎么测我自己的程序」才是这个示例要回答的问题。

与本仓另一套 funq 测试（[tests/gallery_funq](../../tests/gallery_funq/)）的分工：

| | `tests/gallery_funq` | 本示例 |
|---|---|---|
| 形态 | 一条命令跑完的回归测试 | 两个终端各起一边的手动示例 |
| 被测程序由谁拉起 | 用例自己拉起（`add_test` → Python → funq） | **你已经手动起好了**，client 只管连 |
| 靶子 | gallery（qt-libui 仓的示例） | `demo_app`（本仓的产品） |
| 适合 | 卡住 funq 的集成不回归 | 理解 funq 怎么用、摸清自己程序的控件路径 |

## 跑起来

```bat
:: 终端 1 —— server 侧。起 demo_app 并注入 libFunq，然后这个窗口被占住
build\ninja-debug-x64\examples\funq\serve_demo_app.bat

:: 终端 2 —— client 侧。只连 TCP，不启动任何程序
build\ninja-debug-x64\examples\funq\run_demo_client.bat
```

Linux / WSL 同样是这两条，把 `.bat` 换成 `.sh` 即可（由 CMake 生成到构建
目录，带可执行位）。

`Ctrl+C` 结束终端 1，被测程序跟着结束。终端 2 跑完自己退出。

想在别的机器上连，`run_demo_client` 支持 `--host` / `--port`；端口在
`examples/funq/CMakeLists.txt` 的 `FUNQ_DEMO_PORT` 里改，两侧一起换。

## 两个终端分别在做什么

```text
终端 1（server 侧）                        终端 2（client 侧）
──────────────────────────────            ──────────────────────────────
python -m funq_server.runner              FunqClient(host, port)
   │  --port 9000                            │  只是连一条 TCP
   ├─ 启动 demo_app                          │
   ├─ 把 libFunq 注入进去                     │  不知道被测程序叫什么、
   │    Windows：CreateRemoteThread          │  在哪个目录、怎么启动
   │    Linux：LD_PRELOAD                    │
   └─ 一直 wait 着被测程序                    └─ 发消息、收结果

       libFunq 在被测进程里开 TCP 服务
              ↑
              └────────── 双方只有这一条连接 ──────────┘
```

**这就是「分离」的意思**：两边的知识完全不对称。server 侧知道一切（程序在哪、
怎么起、Qt 在哪），client 侧只知道一个 `host:port`。所以用例可以完全不管被测
程序是 exe 还是 AppImage、是 Debug 还是 Release——换个端口就能接管另一个程序。

对照 `tests/gallery_funq` 那条链路就看得更清楚：那边 client 手里还攥着一份
`funq.conf`（写明了可执行文件路径与工作目录），由它负责把程序拉起来。那是
**自动化**需要的，不是 funq 的必需。

## 用例脚本在做什么

`funq_client_demo.py` 七步，每一步都打印它看到了什么：

| 步 | 做什么 | 用到的 API |
|---|---|---|
| 1 | 连上去 | `FunqClient(host, port)` |
| 2 | 认一下登录界面 | 控件路径长什么样 |
| 3 | **故意输错密码**，断言提示文字 | `set_property` / `click` / `properties` |
| 4 | **换对密码**，等主窗口出现 | `widget(path, timeout=...)` |
| 5 | 从菜单触发「设置」 | `action()` / `trigger(blocking=False)` |
| 6 | 读一个数值，改了再改回来 | `set_property` / `wait_for_properties` |
| 7 | 关对话框、断开 | `click` / `close` |

## 三个坑

**① 开模态对话框的动作要 `blocking=False`。**

`Action.trigger()` 默认 `blocking=True`，会一直等到动作执行完。而「设置」开的
是模态对话框、自己不会返回——用默认值会把 funq 自己冻在那一行上。funq 的
docstring 里写明了这一点。

```python
client.action(path=SETTINGS_ACTION).trigger(blocking=False)
```

**② 控件没有 objectName 时，兄弟序号与添加顺序是反的。**

`demo_app` 的控件都有 `objectName`，路径基本能从源码推出来。但这不是必然的
——funq 拼路径的规则是「有 `objectName` 用它，没有就退回类名 + 同名兄弟的
序号」（`libFunq/objectpath.cpp` 的 `_rawObjectName`）。而那个序号与「谁先
添加」是**反的**：

> `setParent()` 会把子对象插到 `children()` 列表的**前面**，funq 的序号按
> `children()` 顺序数。像页面这种「先 `new` 出来、再 `addWidget` reparent
> 进去」的控件，先添加的反倒排在后面。

写这个示例的过程中正好撞上一次：设置对话框那两个页面原本是裸 `QWidget`，
实测下来「**日志**」页叫 `QWidget-1`、「**日志格式**」页才叫 `QWidget`，与
直觉正好相反。现在它们都设了 objectName，路径不再靠猜：

```cpp
auto* page = new QWidget(this);
page->setObjectName(QStringLiteral("settingsPageLog"));
```

**该实测就实测，别推。** 本仓的探针在
[tests/gallery_funq/probe_widgets.py](../../tests/gallery_funq/probe_widgets.py)，
能列出运行中程序的全部控件路径，也能按路径查属性。

**③ 脚本半途失败会把对话框留在屏幕上。**

模态对话框挡住主窗口，看着像程序卡死了。其实没有——再连一次、点一下
`settingsCloseButton` 就能关掉：

```python
client.widget(path="MainWindow::settingsDialog::settingsCloseButton").click()
```

写这些脚本时把「收尾」放在 `finally` 里，能少踩很多次。

## 换成你自己的程序

1. `serve_*.bat.in` / `serve_*.sh.in` 里的 `@DEMO_APP_EXE@` 换成你的程序
   （CMake 变量 `DEMO_APP_EXE`）
2. 你的程序要是也需要 Qt 的库路径，把它加进 `PATH`（Windows）/ `LD_LIBRARY_PATH`
   （Linux）——示例里 Windows 那行就是干这个的
3. 控件路径自己跑一遍探针拿。本仓的探针在
   [tests/gallery_funq/probe_widgets.py](../../tests/gallery_funq/probe_widgets.py)，
   能列出运行中程序的全部控件路径、也能按路径查属性

## 前置条件

- 构建过（`scripts/build_ninja.bat` / `build_ninja.sh`），且 `BUILD_FUNQ` 为 ON
- Python 3.8+，不需要 `pip install` 任何东西——funq 的客户端与服务端源码就在
  `3rdparty/funq-master/{client,server}` 里，由启动脚本挂到 `PYTHONPATH` 上
- 需要真实桌面环境，纯 CI 容器里跑不了；WSL 下靠 WSLg
