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

## 前置条件

**Python 3.8 或更高版本**，仅测试需要，**编译不需要**。

不需要 `pip install` 任何东西：funq 的客户端与服务端源码就在
`3rdparty/funq-master/{client,server}` 里，由包装脚本挂到 `sys.path` 上。上游
`setup.py` 声明的唯一依赖 `nose` 用不到——它只被 funq 的 nose 插件引用，而那个
插件已由 `patches/funq/0004-drop-nose-plugin.patch` 删除（nose 自身在
Python 3.10+ 也已无法运行：依赖被移除的 `imp` 模块）。

## 跑测试

```bat
:: 只跑 funq 用例
ctest --test-dir build\ninja-debug-x64 -R test_gallery_funq --output-on-failure

:: 全套（含单元测试与 QtTesting 回放）
ctest --test-dir build\ninja-debug-x64 --output-on-failure
```

单次约 10 秒：每个用例都要拉起一次程序并等注入完成（funq 在启动后固定
`sleep 1` 秒再注入，见 `funq_server/runner_win.py` 的注释）。

## 机制

```text
FunqTestCase（用例）
   │  按 funq.conf 拉起被测程序：cmd = [funq_attach.bat] + [libui_gallery.exe]
   ▼
funq_attach.bat → python -m funq_server.runner
   │  启动程序 → sleep 1s → 检查进程存活 → CreateRemoteThread + LoadLibrary
   ▼
libFunq.dll（注入进 gallery 进程，在里面起 JSON 服务）
   ▲
   │  TCP：按控件路径操作界面、读属性
   └─ Python 客户端
```

三处看起来绕但都有原因的设计：

- **`funq_attach.bat` 包装**：funq 客户端构造的命令是 `[attach_exe] + [被测程序]`
  （`funq/client.py` 的 `_start_test_process`），所以 `attach_exe` 必须是"吃下后续
  参数再转交 funq runner"的可执行文件；直接拿 `python.exe` 当它是错的，那会把被测
  程序当成脚本名。
- **`libFunq.dll` 被拷到 `funq_server/` 目录**：runner 的 `_find_library` 只认那一个
  位置、那一个名字，而 MSVC 产出的是 `Funq.dll`。拷贝由顶层 `CMakeLists.txt` 的
  `stage_funq_injector` 目标完成（用 `ALL` 目标而非 `POST_BUILD`：解压产物被删除
  重建时对象文件未必重编，`POST_BUILD` 不会再触发，DLL 就悄悄消失了）。
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
set FUNQ_CONF=build\ninja-debug-x64\tests\gallery_funq\funq.conf
python tests\gallery_funq\probe_widgets.py SwitchButton
```

探针会拉起程序、注入、把路径含该关键字的控件全部打印出来。改完界面后重新跑一遍，
把新路径更新到 `gallery.alias`。

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

**构建类型与位数必须一致。** Debug 的 `libFunq.dll` 链 `Qt5Cored.dll`，注入 Debug 的
gallery 才有同一套 Qt 运行时；拿 Release 的库注入 Debug 的程序会在一个进程里加载
两套 Qt。位数同理（`runner_win.py` 走 `CreateRemoteThread`，32 位注不进 64 位进程）。

**需要桌面环境。** 测试要真实窗口与事件循环，纯 CI 容器里跑不了。

## Linux / WSL 侧

机制换成 `LD_PRELOAD`（`funq_server/runner_linux.py`），Python 侧代码完全一样。当前
只做了 Windows 侧：包装脚本与 `libFunq.so` 的暂存需要另配。
