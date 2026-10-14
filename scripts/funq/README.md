# scripts/funq —— 用 funq 看界面

给 AI agent 与人工调试用的探针：启动被测程序并注入 libFunq，然后**按 oid
抓单个控件的像素**。属性只能证明"设了"，像素才证明"生效了"。

解释器用仓库自带的那份（`3rdparty\python-3.8.8-embed-win32\python.exe`），
或任何 python3——`funq_common.py` 会自己把 funq 的两个包挂进 `sys.path`。

## 两终端

```bat
rem 终端 1：起程序（窗口一直占着，Ctrl+C 收工）
3rdparty\python-3.8.8-embed-win32\python.exe scripts\funq\run_demo_app_funq.py -- --set login/animation=tv

rem 终端 2：抓图
3rdparty\python-3.8.8-embed-win32\python.exe scripts\funq\probe_login_anim.py temp\shots tv
```

## 工具

| 文件 | 用途 |
|---|---|
| `run_demo_app_funq.py` | 起 demo_app + 注入；`--` 之后的参数透传给被测程序 |
| `probe_login_flow.py` | 登录端到端：空提交 / 错密码 / 正确凭据 → 主窗口 |
| `probe_login_anim.py` | 左栏动画：隔 1s 连抓 3 帧（证"在动"） |
| `probe_tv_interaction.py` | 电视交互五档逐条验（设计文档 §五 的协议） |
| `probe_eye_button.py` | 密码眼睛按钮两态 |
| `probe_hover.py` | hover 验证：置顶窗口 + 挪真实光标 |
| `add_braces.py` | 移植 qt-libui 场景时给单语句 if/for 补花括号（`--dry` 先看） |
| `funq_common.py` | 公共前戏：挂 sys.path、连、抓图、摊路径 |

## 三条踩出来的规矩

- **x86 靶子必须用 32 位 python 当 runner**（`run_demo_app_funq.py` 按被测
  程序的 PE 位数自动挑）。64 位注 32 位报的是误导性的
  `LoadLibraryW() returned 0 ... Maybe x86/x64 mismatch`
- **判据用像素**：动画、样式、交互状态都没有可读的属性载体——funq 只认
  QWidget，策略对象（如 TvStaticAnimator）拿不到 oid
- **持续态会污染断言**：`typing` 只在回车/失焦时清，而 `keyclick` 不改焦点
  ——每条断言前先"归一化穿梭"（焦点依次给 用户名 → 密码 → 用户名）

## hover 的两个坑（`probe_hover.py` 已把整套固化）

1. **目标窗口必须置顶**：被别的窗口（终端）压着时，光标挪过去了事件也
   到不了。`SetWindowPos(HWND_TOPMOST)` 提到最前、用完还原
2. **坐标要算准**：Qt 顶层窗口的 `x()/y()` 给的是"帧原点"，子控件坐标却是
   相对**客户区**的，中间差一个标题栏（实测 39px，够让光标落到隔壁控件）。
   用 `ClientToScreen` 拿客户区原点，再逐级叠加子控件的 `x/y`

细节与排错另见 [funq 注入式 UI 测试](../../docs/guide/funq注入式UI测试.md)。
