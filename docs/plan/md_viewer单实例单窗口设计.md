# md_viewer 单实例单窗口设计

## 问题

md_viewer 目前每启动一次都会新建一个进程和窗口。双击多个 Markdown 文件会
留下多个进程，它们共享同一份 `md_viewer.ini`，最近文件、窗口几何和 dock
布局会互相覆盖。需要在启动入口加单实例约束。

demo_app 已经有单实例单窗口实现，本次只处理 md_viewer。

## 方案选择

### 单实例单窗口（采用）

- 首次启动持有本地端点，正常建窗口。
- 后续启动把命令行参数转发给首实例，然后直接退出。
- 首实例收到参数后调用 `OpenPath()`，把新文件/目录放到现有窗口里，再置前。
- 没有参数时只激活现有窗口。

理由：

- md_viewer 本来就是单文档窗口，导航历史、搜索、导出、最近文件都围绕一个
  当前文档组织。
- 复用现有 `SingleInstance` 的参数转发和窗口激活能力，不需要窗口注册表。
- 设置模型只有一份，不会引入多窗口几何/最近文件的合并策略。

### 单实例多窗口（暂不做）

需要窗口注册表、每窗口的设置与几何策略，以及“最后一个窗口关闭才退出”的
生命周期；`SingleInstance` 当前只有一个 `activationWindow`，多窗口还要扩展
激活目标。等确实出现同时对照多篇文档的需求再做。

## 目录列表语义

保持现有的单目录模型：一个窗口只有一个文件列表和一个 `listed_dir_`。第二
实例打开另一个目录时，替换列表并打开该目录第一篇 Markdown，不保留旧目录
节点，也不改 `QDockWidget`。当前文档的 `base_dir` 仍由 `MarkdownView` 按
文档设置，图片和相对链接不受目录切换影响。

如果以后要在同一窗口同时保留多个目录，再把列表换成目录树；本次不做。

## 单实例组件

复用 `src/core/common/single_instance.{h,cpp}`，不复制一套协议：

- `appKey = QCoreApplication::applicationName()`，与 funq 探测端点一致。
- Windows 命名互斥体使用 `md_viewerMutex`；本地套接字仍然负责参数转发。
  安装脚本当前没有 `AppMutex` 判据，这个名字只服务于运行时；若以后安装器
  要检测运行中进程，再补同名 `MyAppMutex`。
- `connectToExistingInstance()` 在原有 1 秒超时内短间隔重试，覆盖首实例
  “已建互斥体、尚未开始监听”的窗口。两个实例同时双击启动时不丢参数。
- 把 `single_instance` 从 core 的源文件列表拆成独立静态库目标
  `${APP_TARGET_PREFIX}_single_instance`，core 与 md_viewer 都链接它。
  这样 md_viewer 不会因为单实例把 auth、配置、崩溃捕获整套 core 拖进来。
  拆出时必须同时从 core 的 `ALL_HEADERS_SOURCES` 删除
  `common/single_instance.cpp/.h`，不能两边各编一份。

拆出的目标还需要：

- `Qt5::Core`、`Qt5::Network`；
- `${APP_TARGET_PREFIX}_log`（`single_instance.cpp` 使用 `LOG_*` 宏）；
- `AUTOMOC ON`（`SingleInstance` 有 `Q_OBJECT`）；
- 公开 `src/core` 作为 include 目录；
- Windows 下显式链 `user32`（`single_instance.cpp` 的激活代码使用 Win32 API）。

## 启动流程

```text
QApplication
  -> 设置 applicationName = md_viewer
  -> 创建 SingleInstance(appKey, mutexName)
  -> isAppAlreadyRunning()?
       是：转发完整 arguments -> 退出 0
       否：startListening()
            失败且此时已有实例：转发 arguments -> 退出 0
  -> 读取 MdViewerSettings
  -> 建 MdViewerWindow、setActivationWindow、连接信号
  -> show()
  -> 第一个位置参数：OpenPath；没有则 OpenLastFile
  -> app.exec()
```

首实例的 `newInstanceLaunched` 处理：

1. 跳过 argv[0]，取第一个非选项位置参数，转成绝对路径。
2. 路径非空则 `window.OpenPath(path)`。
3. 无路径则不做文档操作。
4. `showApplication` 统一负责 `showNormal()`、`raise()`、`activateWindow()`。

第二实例转发 `QCoreApplication::arguments()` 的完整列表，不剥掉 argv[0]。
这样即使没有文件参数，首实例也能通过非空参数列表进入
`handleSocketReadyRead()`，随后触发 `newInstanceLaunched` 与激活；
首实例解析路径时必须跳过 argv[0]。相对路径按第二实例自己的当前目录转成
绝对路径，避免首实例用另一个工作目录解析；目录参数同样处理。

## 边界与失败处理

- 已有实例但连接失败：重试到原有超时后，以 0 退出，不再抢端点；日志记录
  失败原因。不能静默起第二个进程。
- `startListening()` 失败：先重探是否已有实例；有则转发后退出，没有才继续
  启动窗口。仅在没有可用端点时降级为“无单实例保护”，不能让第二个 UI 在
  已有实例时启动。
- 第二个实例带多个位置参数：当前 md_viewer 只支持单文档，只打开第一个。
- 文件不存在：仍把路径转给首实例，由首实例的错误页负责提示，不在第二实例
  静默丢弃。
- 打开没有 Markdown 的目录：保留原目录列表与当前文档，只提示该目录为空；
  避免列表被清空但旧文档还留在屏幕上。
- 首实例正在渲染：`OpenPath` 走现有取消/替换流程，不新增状态。
- funq 测试脚本：单实例生效后，启动被测程序前要先确认没有残留实例。现有
  `examples/funq-scripts/check_no_instance.py` 只服务 demo_app；md_viewer 若
  接入 funq，需要单独的启动器在启动前传 `--name md_viewer` 调用它。

## 测试与验证

- 构建：`scripts/build_ninja.bat -t relwithdebinfo -a x86`。
- 手工/脚本验证：
  1. 启动首实例并打开 A.md。
  2. 再启动一个 md_viewer B.md；第二进程应快速退出，首实例窗口切到 B.md。
  3. 第二个实例不带参数；只激活首实例，不改变当前文档。
  4. 第二个实例传目录；首实例铺开该目录。
  5. 第一个实例最小化时再次启动；窗口恢复并置前。
- 现有 Markdown 分组测试不受影响；单实例链路用 funq/进程级脚本验证，不塞进
  `tests/markdown`。

## 涉及文件

- `src/core/CMakeLists.txt`：拆出 single-instance 目标，并从 core 源列表移除。
- `src/core/common/single_instance.cpp`：连接重试。
- `examples/md_viewer/CMakeLists.txt`：链接 single-instance 目标。
- `examples/md_viewer/main.cpp`：检测、监听、参数归一化、窗口接线。
- `examples/md_viewer/md_viewer_window.*`：不需要新增公开 API；继续用
  `OpenPath()`。
- `docs/plan/`：本设计文档。

## 风险

- 首实例启动与 `startListening()` 之间存在极短竞态；本地端点先建立、窗口
  后建，期间收到的连接在事件循环启动后处理，不丢参数。连接端在 1 秒超时内
  重试，覆盖同时双击多篇文档的场景。
- Unix 上 `startListening()` 开头的 `removeServer()` 本身有 TOCTOU；本次沿用
  core 现有实现，不在示例层重写。若以后出现 Linux 多开竞争，再单独修 core。
- `single_instance` 拆分后要重新确认 core 与 md_viewer 的 32 位构建。
- README/CHANGELOG 里如有 md_viewer 启动说明，需要补一句“只允许一个实例”。
