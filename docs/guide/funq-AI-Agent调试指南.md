# funq 给 AI Agent 的 UI 调试指南

funq 是注入式的 Qt UI 探针：在被测进程里开一个 TCP 服务，Agent 用 Python
读控件属性、模拟点击/快捷键、按控件抓像素。它不要求被测程序为测试改代码。

一句话原则：**属性只能证明"设了"，像素才能证明"生效了"。**

更完整的机制、用例写法和排错见
[funq 注入式 UI 测试](funq注入式UI测试.md)。本文只讲 Agent 调试时的最短路径
和已经踩过的坑。

## 适用场景

适合：

- 验证界面状态：主题、可见性、选中态、当前项、计数。
- 验证交互结果：点击某个按钮后另一个控件是否变化。
- 定位控件路径：不知道控件在哪、objectName 是什么。
- 检查像素效果：颜色、圆角、模糊、动画、中文字体。
- 读取 Qt 侧的 Q_PROPERTY 做断言。

不适合：

- 读取非 QObject（策略对象、纯 C++ 类）的内部状态。
- 动态 `setProperty()` 建的属性：`properties()` 读不到，也不报错。
- 直接调用普通方法：只认槽与显式暴露的入口，见下文。
- 性能分析、崩溃定位：用日志、minidump 和调试器。

## 前置条件

1. 构建时 `BUILD_FUNQ=ON`（默认开），产物里要有 `Funq.dll`。
2. 靶子与注入库位数一致；x86 靶子必须用 32 位解释器当 runner。
   仓库自带的是 `3rdparty/python-3.8.8-embed-win32/python.exe`。
3. 客户端可以用任意 Python 3；只有 runner 受位数限制。
4. Qt 运行时：从构建目录的 `run_<target>.bat` 里抄 `PATH` 与
   `QT_PLUGIN_PATH`，不要手拼。
5. 确保没有同名实例在跑：已有实例会吞掉新进程，runner 随后报一个看不懂的错。
6. 端口每轮换一个，避免上一轮残留。

## 启动：单脚本模板

Agent 不适合开两个终端。用 `subprocess` 起 runner，再连客户端，最后收尾：

```python
import os
import subprocess
import sys
import time

REPO = r"D:/work/repo"
BUILD = os.path.join(REPO, "build", "ninja-relwithdebinfo-x86")
BIN = os.path.join(BUILD, "bin")
PY = os.path.join(REPO, "3rdparty", "python-3.8.8-embed-win32", "python.exe")
CLIENT = os.path.join(REPO, "3rdparty", "funq-master", "client")
SERVER = os.path.join(REPO, "3rdparty", "funq-master", "server")
PORT = 9011

env = dict(os.environ)
env["PATH"] = r"D:/Qt/5.15.2/msvc2019/bin;" + env.get("PATH", "")
env["QT_PLUGIN_PATH"] = r"D:/Qt/5.15.2/msvc2019/plugins"
env["PYTHONPATH"] = os.pathsep.join([CLIENT, SERVER])

sys.path.insert(0, CLIENT)
from funq.client import FunqClient

cmd = [PY, "-m", "funq_server.runner", "--port", str(PORT),
       "--library", os.path.join(BIN, "Funq.dll"),
       os.path.join(BIN, "your_app.exe")]
proc = subprocess.Popen(cmd, env=env, cwd=BIN)
try:
    client = FunqClient(port=PORT, timeout_connection=40)
    time.sleep(1.5)  # 等窗口建完
    # 探针逻辑
    client.quit()
finally:
    try:
        proc.wait(timeout=15)
    except Exception:
        proc.kill()
```

要点：

- runner 必须用与被测程序同位数的 Python，客户端不受限。
- `--library` 指向本构建目录的 `Funq.dll`，不要用源码树里暂存的那份。
- 靶子参数放在 exe 之后，直接透传。
- 结束时先 `client.quit()`，再等 runner；进程残留会锁住 DLL，下一轮构建失败。

## 找控件路径

不要猜路径。先摊平整棵树：

```python
def flatten(tree, prefix=""):
    out = []
    for name, node in tree.items():
        path = "%s::%s" % (prefix, name) if prefix else name
        out.append(path)
        out.extend(flatten(node.get("children", {}), path))
    return out

for path in flatten(client.widgets_list()):
    print(path)
```

规则：

- 路径优先用 `objectName`；没有时退化成"类名 + 同名兄弟序号"。
- 序号依赖创建顺序和布局重挂父对象，**只用于当次运行**，别写进长期用例。
- 经常要探的控件，在目标程序里补一个 `objectName`，它是 funq 的稳定锚点。
- `client.widget(path=...)` 按完整路径取；取到后 `.oid` 用于抓图与命令。
- 菜单项是 `QAction`，不是 QWidget；用 `client.action(path=...)` 触发。

## 读状态

```python
widget = client.widget(path="MainWindow::someObjectName", timeout=10.0)
print(widget.properties())
```

常见可读项：`visible`、`enabled`、`text`、`checked`、`currentIndex`、
`currentText`、`count`、`value`、`x/y/width/height`。

限制：

- 只认静态 `Q_PROPERTY`。运行时 `setProperty()` 建的动态属性读不到。
- 没有 `invoke_method` 这种通用入口。`call_slot` 只认槽，并且固定一个
  `QVariant` 入参；无参槽、普通 public 方法都调不到。
- 需要 Agent 钩子时，在目标类加 `Q_INVOKABLE QVariant xxx(const QVariant&)`。
- funq 的类会沿继承链回退（例如 `QListWidget` 退到 `QAbstractItemView`），
  所以"注册表里没有"不等于"读不到"。

## 操作控件

### 点击

```python
widget.click()          # 服务端点击 rect().center()
widget.set_property("checked", False)  # 确定性地改状态，会走 toggled
```

`click()` 的坐标是控件中心。**宽控件会点空**：一个被布局拉满 295px 的
Fluent 开关，开关本体只在左侧约 40px，中心落在文字外，点击不会切换。
需要真实点击时，把局部坐标映射到屏幕，再用系统鼠标事件：

```python
import ctypes
user32 = ctypes.windll.user32
gx, gy = widget.map_position_to(10, 17, None)
user32.SetCursorPos(gx, gy)
user32.mouse_event(0x0002, 0, 0, 0, 0)  # LEFTDOWN
user32.mouse_event(0x0004, 0, 0, 0, 0)  # LEFTUP
```

### 快捷键

```python
client.send_command("shortcut", keysequence="F3", oid=window.oid)
```

这条命令用 `QTest` 直接把按键送到目标窗口，适合验"动作接线"。
要验真实输入链路（焦点、输入法、全局热键）时，改用 `keybd_event` /
`SendInput`，并先用真实鼠标点一下窗口确保它在前台。

**假阴性警告**：第三方全局热键会吞掉按键。实测安装 Snipaste 后，
Shift+F3 在系统层就被截走，应用完全收不到；退出后恢复正常。遇到
"某个快捷键死活不触发"，先确认没有全局热键软件，再怀疑代码。

## 看像素

按 oid 抓单个控件：

```python
import base64
data = client.send_command("grab", oid=widget.oid, format="PNG")
open("temp/shot.png", "wb").write(base64.standard_b64decode(data["data"]))
```

- 抓的是该控件自身渲染的位图，窗口被压住也拍得到。
- 只包含这棵子树；对话框、独立顶层窗口要各自抓。
- **DWM 模糊、窗口透明、屏幕合成效果不在控件位图里**。这类效果要用
  不带 oid 的整屏抓图：

```python
data = client.send_command("grab", format="PNG")
```

验收不要靠肉眼看缩略图。用 PIL/Numpy 采样关键像素或统计差异：

```python
from PIL import Image
a = Image.open("temp/off.png").convert("RGB")
b = Image.open("temp/on.png").convert("RGB")
print(a.getpixel((600, 20)), b.getpixel((600, 20)))
```

实例：主题切换后标题栏背景从 `(253,253,253)` 变到 `(52,52,52)`；
DWM 模糊开关前后同一位置约 `(243,243,243)` 与 `(165,164,162)`。
文档区域应保持 `(255,255,255)` 不变，用来确认只改了壳。

## 读 Windows 原生状态

Qt 属性覆盖不到的原生状态，用 `ctypes` 直接查。例如验证置顶按钮：

```python
import ctypes
user32 = ctypes.windll.user32
hwnd = user32.FindWindowW(None, "窗口标题")
style = user32.GetWindowLongW(hwnd, -20)  # GWL_EXSTYLE
print(bool(style & 0x8))                  # WS_EX_TOPMOST
```

点击前后各查一次，确认 `False -> True -> False`。

## 常见假阴性清单

| 现象 | 先查什么 |
|---|---|
| 注入失败，报 x86/x64 mismatch | runner 的 Python 位数是否与靶子一致 |
| 程序没起来 / runner 卡住 | 是否已有同名实例；端口是否被占 |
| 控件路径这次有、下次变 | 路径靠类名+序号，补 objectName |
| 属性读不到 | 是不是动态属性；是不是非 QWidget |
| 点击无效 | 是否点在控件中心之外；改用真实鼠标坐标 |
| 快捷键无效 | 是否有全局热键软件；焦点是否在目标窗口 |
| 抓图看不到模糊/透明 | 用整屏 `grab`，控件抓图不含合成效果 |
| 下一轮构建失败 | 上一轮的被测进程还在，DLL 被占用 |

## 收尾

```bash
taskkill //F //IM your_app.exe
```

- Agent 的临时脚本放在工作区 `temp/`，不要放系统 TEMP。
- 抓图与中间产物也放 `temp/`，不要入库。
- 一轮调试结束先杀进程，再改代码、再构建。

## 现成工具

| 位置 | 用途 |
|---|---|
| `scripts/funq/funq_common.py` | 挂 sys.path、连接、按路径抓图、摊平树 |
| `scripts/funq/run_demo_app_funq.py` | 起 demo_app + 注入，按 PE 位数挑解释器 |
| `scripts/funq/probe_*.py` | 登录、动画、hover、Markdown 预览等现成探针 |
| `scripts/funq/README.md` | 两终端用法与 hover 坐标坑 |
| [funq 注入式 UI 测试](funq注入式UI测试.md) | 机制、funq.conf 用例、排错 |
| [调试 libFunq](调试libFunq.md) | 注入库本身的调试 |
