# guidance 接入 funq

把引导的演示层当成 funq 执行的**可视化层**：脚本每动一个控件，屏幕上就能看见
高亮框指到它。同一套渲染层平时给用户当新手引导，跑脚本时给自动化当"看得见的
光标"。

## 一、矩形从哪来（试验结论）

bridge 要一个**全局坐标矩形**才能高亮。两条路，实测后选了第二条。

**实测**（注入 demo_app，读已知控件的属性）：

| 想拿 | 结果 |
| ---- | ---- |
| `geometry` / `pos` / `size` | **拿不到**——QtJson 序列化不了 QRect/QPoint/QSize，`dump_properties` 直接跳过 |
| `x` / `y` / `width` / `height` | 拿得到，都是 int |
| 但 `x`/`y` 是**父相对**的 | `MainWindow` 报 696/263（屏幕坐标），`pagePlaceholderTitle` 报 9/359（相对中央控件） |

Python 侧要凑出全局坐标得沿父链累加，依赖"父链完整 + 每层都是 Qt 客户端坐标"，
脆。

**决定：C++ 侧解析 path，但不依赖 funq。**

bridge 收 **path 字符串**，自己用 Qt 的 `findChild` 按最后一段 objectName 找控件
（从 `QApplication::topLevelWidgets()` 逐个递归），再用引擎内部那套 `mapToGlobal`
算矩形。好处：

- **零 funq 依赖**——guidance 库保持干净，不用链 funq
- **与引擎同源**——引导自己定位高亮用的就是这个算法，不会出现"算出来的框和引擎
  画的对不上"
- Python 侧只传字符串，沿用 funq 已有的 path 概念，不引入新概念

**解析器的两条限制**（实现并实测后确认，脚本里要绕开）：

| 限制 | 后果 | 绕法 |
| ---- | ---- | ---- |
| 只认**最后一段的 objectName** | `MainWindow::settingsDialog::settingsCloseButton` 能解析；`...settingsRow-1::QSpinBox` 不能——`QSpinBox` 是 funq 按"类名-序号"造的合成名，不是 objectName | 给那个控件补 objectName，或改指有名字的上层控件 |
| 目标是 **QWidget**（引擎的目标类型只有 控件/矩形/图） | 菜单项是 `QAction`，指不了 | 改指它所在的菜单（菜单本身是 QWidget） |

实测里这两条各撞了一次：`examples/funq/funq_client_demo.py` 的第 5、6 步因此改指
「工具」菜单与「日志」页。

## 二、五项决定

| # | 问题 | 决定 |
| - | ---- | ---- |
| 1 | 矩形从哪来 | C++ 侧按 path 解析（见上） |
| 2 | 模态对话框的层级 | **不设 Qt 父子**，手动 `raise()` + `setGeometry()` 管理层级 |
| 3 | 被动模式的边界 | 三个按钮全隐、倒计时**一律停**、点遮罩不退出；**气泡可关**（只想高亮不想说话时） |
| 4 | funq 怎么找到 bridge | bridge 是带 `objectName` 的**可见 QWidget**（实测：纯 QObject 会被判 `NotAWidget`），走 `call_slot` |
| 5 | 接进哪些脚本 | `examples/funq/funq_client_demo.py` 七步加 highlight；**ctest 不动** |

第 2 条的背景：presentation 原先靠 `setParent` 换宿主，而模态框一关宿主被销毁、
presentation 会被 Qt 的父子机制一起带走。改成不设父子后，`raise()` 保证它压在
模态框之上，`setGeometry()` 跟着宿主窗口走。**样式不受影响**——样式已经是应用级
的了（见 `guidance_style.h`）。

## 三、被动模式加在演示层上

加在 `GuidancePresentation`（不是 controller）：它才是渲染层；被动模式纯粹是
"外部驱动、只显示不交互"，与主题/步骤的推进逻辑无关。

1. `skip_btn_` / `prev_btn_` / `next_btn_` 全 `hide()`
2. `count_down_timer_->stop()`
3. `mousePressEvent` 里"点遮罩空白退出"加 `if (passive_mode_) { return; }`
4. 气泡可关：关掉就只有高亮框，没有文案

## 四、bridge 就是 GuidanceSupport

**不新增类。** `GuidanceSupport` 的 `showHighlight` / `hideHighlight` 已经把位置
留好了（TODO 里管它叫 `GuidanceBridge`）。按下第一条的决定，它不需要依赖 funq。

**但它必须是个可见的 QWidget，而且得早于登录就存在**——两条都是实测撞出来的：

1. funq 的 `call_slot` 走 `WidgetLocatorContext<QWidget>`，即
   `qobject_cast<QWidget*>`，纯 `QObject` 会被判成 `NotAWidget`；而且
   `client.widget()` 默认还要等控件 `visible`。所以本类**基类是 QWidget**，
   并且构造里要 `show()`。它没有任何界面，做成 0 尺寸、不挡鼠标的一块占位控件
2. funq 脚本的第一步就跑在**登录对话框**上，那时主窗口还没创建。挂在主窗口下的话
   那几步没法高亮，所以它是**独立顶层窗口**（`Qt::Tool` 不进任务栏），路径就是
   光秃秃的 `guidanceBridge`

签名受 funq 限制：它固定按
`QMetaObject::invokeMethod(obj, name, DirectConnection, Q_RETURN_ARG(QVariant), Q_ARG(QVariant))`
调用，**必须**是"单个 QVariant 进、QVariant 出"——拆成多个形参 invokeMethod 匹配
不上，直接返回 `NoMethodInvoked`。

接口：

```cpp
class GuidanceSupport : public QWidget {
  Q_OBJECT
 public:
  Q_INVOKABLE QVariant showHighlight(const QVariant& params);
  Q_INVOKABLE QVariant hideHighlight(const QVariant& params);
};
```

参数 `{"path": …, "title": …, "text": …}`；返回 `{"ok": bool, "message": str}`，
解析失败时脚本能自己判断，不会静默。

Python 侧：

```python
guide.highlight(LOGIN_PASSWORD, "密码", "这一步故意填错")
client.widget(path=LOGIN_PASSWORD).set_property("text", BAD_PASSWORD)
client.widget(path=LOGIN_BUTTON).click()
guide.clear()
```

## 五、验证顺序

逐段验，每段都能独立看出对错：

1. **通道** — Python 调 `hideHighlight`，看 C++ 侧有没有进函数
2. **解析** — 传一个已知 path，看能不能找到控件
3. **画出来** — 屏幕上有没有遮罩 + 高亮框
4. **模态** — 在设置对话框上高亮，看层级对不对（验第 2 条决定）
5. **接脚本** — `funq_client_demo.py` 七步各加一次 highlight

顺带一个演示才需要的：**每步之间要停留**。funq 没有慢放开关（`DelayedResponse`
是给异步响应用的，客户端只有"等条件"的 `wait_for_properties`，挡不住高亮一闪而
过），而脚本本身是毫秒级的。所以停留放在 `Guidance.highlight()` 里，`--dwell`
可调、设 0 关掉——真跑自动化时关掉。

## 六、不在这次范围

- **ctest 用例不加高亮**：无人看，只会拖慢跑时并引入脆弱点
- `GuidanceHomePage` 与被动模式无关，不动
