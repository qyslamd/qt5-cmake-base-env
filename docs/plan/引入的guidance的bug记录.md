# 引入的 guidance 模块 bug 记录

记录 `src/app/guidance/` 中已确认的缺陷。每条带证据（文件:行）与修法。

**背景**：该模块是外部移植进来的。接入主界面（菜单 → 主页 → 播放）之前它
**未被任何地方引用**，既不在构建产物里跑，也没有测试覆盖，所以下面这些问题
不会被现有流程暴露。接线后逐条显形。

## 状态

| 编号 | 问题 | 状态 |
| ---- | ---- | ---- |
| B1 | 跨主题时"自动模式"不同步 | **已修** |
| B2 | 实操式主题里仍会自动翻页 | **已修** |
| B3 | 倒计时进度条起始为 0 宽、秒数缺 "5" | **已修** |
| B4 | 第二遍运行气泡不见了 | **已修** |

B1、B2 是同一个根因的两面，一并修掉：把"推模式给演示层"收进
`ApplyFlowOptions()`——三条路径（`execute` / `onNextClicked` / `onPrevClicked`）
都要经过它，不再像以前那样只有 `execute()` 记得推。

B2 还带一个状态泄漏：`ApplyFlowOptions` 原先只会把 `auto_mode_` 置 false，全仓
没有一处置回 true，于是走过一个实操主题后自动模式**永久失效**。改法是分开两个
概念——`requested_auto_mode_` 记用户请求的模式，有效模式
`auto_mode_ = requested_auto_mode_ && !flow->interactive()` 现算。这样实操主题
按设计降为手动，离开它之后讲解主题能回到自动。

这两条只有在**多条主题**、且其中一条是实操式时才会显形——现在
`examples/guidance_demo` 有三条主题（实操式夹在中间），跑一次「全部演示」即可
覆盖。

## B1 跨主题时"自动模式"不同步

**现象**：跨主题前进或回退之后，控制器与演示层对"是否自动播放"的认知会分
叉，而且是静默分叉。

**证据**：`setIsAutoMode` 全仓**只有一个调用点**——`guidance_controller.cpp:55`，
在 `execute()` 里。而 `execute()` 只在启动时走一次；跨主题的两条路径都只调
了 `ApplyFlowOptions`，没调它：

- 前进：`guidance_controller.cpp:190`（`onNextClicked`）
- 回退：`guidance_controller.cpp:225`（`onPrevClicked`）

**根因**：`refreshStepPresentation()` 末尾判断要不要起倒计时定时器，看的是
**演示层自己的** `auto_mode_`（`guidance_presentation.cpp:229`）。跨主题时这
个值没人更新，于是停留在上一个主题留下的状态。

**影响**：跨主题后自动/手动行为随机——取决于上一个主题是不是实操式。

**修法**：把"推模式给演示层"收口成一个 `syncPresentationMode()`，在三条路
（`execute` / `onNextClicked` / `onPrevClicked`）都调一次，并与
`ApplyFlowOptions` 绑在一起，免得以后再加路径又漏。

## B2 实操式主题里仍会自动翻页

**现象**：用户在实操式主题里动手操作，5 秒一到页面自己跳走。

**证据**：`ApplyFlowOptions`（`guidance_controller.cpp:65-74`）会把控制器侧的
`auto_mode_` 置 false，但**不会推给演示层**；紧接着的
`presentation_->setIsAutoMode(...)` 只存在于 `execute()`，跨主题路径没有
（见 B1）。于是 `setInteractive(true)` 已经把主题设成"等用户动手"，而演示层
的 `auto_mode_` 还是 true → 定时器照旧启动。

**影响**：鼠标穿透（实操）与自动翻页两个模式**同时开着**，正是设计上要互斥
的一对。`ApplyFlowOptions` 的注释写明意图是"实操式主题忽略自动倒计时：等用户
动手优先于自动翻页"，实现没做到。

**附带的状态泄漏**：`ApplyFlowOptions` 只会 `auto_mode_ = false`，全仓**没有
一处把它置回 true**。所以一旦经过一个实操主题，控制器侧的自动模式**永久
失效**。而演示层侧因为跨主题不推送，反而可能还停在 true——两边各错各的。

**修法**：随 B1 一并解决。另需决定语义：经过实操主题后，后续讲解主题要不要
恢复自动？本记录倾向于"要恢复"（`startAll(true)` 的意图是自动播全部），但这
属于设计决策，需与用户确认。

## B3 倒计时进度条起始为 0 宽、秒数缺 "5"

**现象**：两个视觉瑕疵，同一个根因。

1. 进度条第一帧是**空的**，30ms 后瞬间跳满，再慢慢排空
2. 秒数标签第一帧**无文本**，之后从 **4** 开始，`5` 从没出现过

**证据**：

```cpp
// guidance_presentation.cpp:229-233（起表）
  if (auto_mode_) {
    time_percent_ = 1.0;          // ← 起点给反了
    count_down_tick_ = 0;
    count_down_timer_->start(30);
  }

// guidance_presentation.cpp:302（画条）
  rect.setWidth(rect.width() * (1 - time_percent_));

// guidance_presentation.cpp:153-154（刷秒数，只在 tick 里赋值）
  int seconds = 5 * (1 - time_percent_);
  countdown_label_->setText(QString::number(seconds));
```

**根因**：`time_percent_` 的语义是"已过去的比例"（起点应为 0.0），代码给了
1.0。于是起表那一帧：条宽 `W × (1 - 1.0) = 0`，秒数若当时渲染会是 0。第一个
tick 之后 `time_percent_ ≈ 0.006` → 条宽 ≈ 满、秒数 `int(5 × 0.994) = 4`。

另外起表那一帧**没有同步刷新文案与重绘**，所以标签是空的。

注释声称"与文案 5..0 一致"，实际是「空白 → 4 → 3 → 2 → 1 → 0」。

**影响**：纯视觉，不影响翻页时序（定时器 166 tick × 30ms ≈ 4980ms 是对的）。

**修法**（已实施）：`time_percent_` 的初值与成员默认值都改成 `0.0`，起表那一帧
同步写一次秒数并重绘；写死的 `5` / `5000` / `30` 提成 `kCountDownSeconds` /
`kCountDownTickMs` / `kCountDownMaxTicks`，三者同源。

## B4 第二遍运行气泡不见了

**现象**：同一条主题连播两次，第二遍只剩遮罩与高亮，**气泡不出来**。

**证据**：`hideEvent` 无条件显式隐藏气泡：

```cpp
// guidance_presentation.cpp（hideEvent）
  if (bubble_widget_ != nullptr) {
    bubble_widget_->hide();
  }
```

而 `showEvent` 只在"气泡没挂在遮罩上"时才恢复它：

```cpp
// guidance_presentation.cpp（showEvent，修复前）
  if (bubble_widget_ != nullptr && bubble_widget_->parentWidget() != this) {
    bubble_widget_->setVisible(true);
    bubble_widget_->raise();
  }
```

讲解模式下气泡**就是**挂在遮罩上的（`parentWidget() == this`），恢复分支进不去。

**根因**：Qt 里被**显式** `hide()` 过的子控件，父控件 `show()` 不会把它带回来
——`QWidgetPrivate::showChildren()` 会跳过 `isHidden()` 的子控件。原注释把
"挂在遮罩上"误当成"跟着父级显隐就行"，漏掉了显式隐藏这条。

第一遍之所以正常：气泡从未被显式隐藏过，随遮罩首次 `show()` 一起出来。

**修法**（已实施）：`showEvent` 里去掉 `parentWidget() != this` 这个条件，无条件
恢复气泡（`raise()` 本来两种情况都要）。

## 附：两处规范问题

与功能无关，但接线前顺手清掉更省事：

1. **`guidance_config.h:1` 用了 `#pragma once`** —— 项目规则禁止，需换成
   `FILE_GUIDANCE_CONFIG_H_` 形式的宏保护（同目录其余四个头文件已是宏保护）
2. **`guidance_config.cpp` / `guidance_controller.cpp` /
   `guidance_presentation.cpp` 用的是 `qWarning()`** —— 项目规则要求尽量走
   封装好的 `Logger` 宏

另注：`guidance_config.cpp:12,16` 的告警文案为 `"foolish! ..."`，措辞不像本
仓手笔，疑为外部移植。若是，则链式构造器（`withTitle` / `withStep` /
`addFlow`）那套 API 风格也与本仓其余部分不一致，建议确认是保留还是改写。
