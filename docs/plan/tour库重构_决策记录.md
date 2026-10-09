# tour 库重构 —— 关键决策记录

**这份记录是什么**：`src/tour`（引导/导览库，qt-libui 的 fork）按一次外部评审
重构的决策台账，外加**从代码注释里搬出来的那些坑**。代码里只留"为什么要有它"
（每条 ≤3 行），"当年那场事故怎么发生的"记在这儿——读者要接手时看这份，读代码
时不必考古。

- 交接与步骤表：`temp/tour_重构交接.md`（工作区 temp/，不入库）
- 设计文档：《一步多靶设计》在产品线的 `docs/plan/plan总结.md`（模板线没有这一节）
- 定稿日期：2026-10-09

---

## 一、评审四条与处理

### D1 TourOverlay 是上帝类（759 行）

**当时的选项**：① 只挪函数、类不动；② 按"谁画什么"拆成多个类；③ 按"谁被谁
调用"分层。

**选定 ②**，拆出四个各管一件事的类 + 一个协调者：

| 类 | 管什么 |
| --- | --- |
| `TourOverlayMask` | 高亮**在哪儿**：挖洞绘制、命中判定、高亮动画 |
| `TourBubbleView` | 气泡自己：布局、自绘底/箭头/倒计时条、换挂法、显隐 |
| `TourCountdown` | **还剩几秒**：timer、已走比例、剩余秒数 |
| `TourBubblePlacer` | 气泡摆哪儿（纯几何，可单测），第 2 步就抽走了 |
| `TourOverlay` | 协调：模式三态、`updateUi`、`showHighlight*`、`setHost`（368 行） |

**为什么不是 ③**：按调用关系分层会切出"上半截类 + 下半截类"，两边都要知道
绘制细节。按"谁画什么"切，每块都能单独看懂、单独改。

**坐标与锚点的归属**：靶由 `TourOverlay` 按次递给气泡（`followAnchor(anchor,
viewport)`），气泡不持有遮罩——"箭头画不画"与"气泡摆哪儿"因此读的是同一个
矩形，不会有第二处口径。

### D2 `use_anime_` 是恒真死配置

一个 `bool use_anime_ = true;` 的成员，没有 setter、没有赋值，读它的四个分支
全是死代码——留着的作用只是让后来的人去找那个不存在的开关。删掉，动画一律走。

### D3 高亮状态有三份（注释里写着"一处真相"，实际不是）

`highlight_.anchor` + `highlight_.extras` + `anim_anchor_rect_`，靠约定同步。
漏同步的后果是**幽灵框**：多靶那条路忘了同步动画副本，锚点框停在上一个靶的
位置上，而 `highlightCount` 照样报数，用例抓不住。

现在：`Highlight{ anchor, extras }` 是**要去的地方**，`shown_anchor_` 是动画
**此刻显示中的框**（明确标注它不是第二份真相，只用于绘制与命中）。

### D4 `Target = std::variant<QWidget*, QRect, QPixmap>` 类型上是错的

三支里"矩形必须是全局坐标"只能写在注释里靠调用方记得；"额外靶不支持图片"
只能用一个 `qWarning` 挡。

现在按**内容**分型：`WidgetTarget` / `GlobalRect` / `SplashImage`；额外靶只收
`Target = variant<WidgetTarget, GlobalRect>`——图片压根进不来，成了类型保证，
那个 qWarning 连同它的分支一起消失。

### D5 命名

- 库内**方法**统一小驼峰（`clearFlows`→`clearTopics`、`actionTemplateTour`→
  `actionTour`、`has_prev_flow`→…），类名一律 `Tour` 前缀（`TourOverlayMask` /
  `TourBubbleView` / `TourCountdown`）。
- 编译期常量一律 `static constexpr`，不占实例空间。

---

## 二、当年那些坑（按主题）

### 状态与模式

1. **实操主题的"遮罩穿透"从来没生效过。** `setInteractive` 与 `setPassiveMode`
   各写一句同一个 `WA_TransparentForMouseEvents`，而控制器里是连着调的
   （`applyTopicOptions`：先 `setInteractive(topic->interactive())`，紧跟
   `setPassiveMode(false)`），后者无条件把前者刚设的 `true` 盖回 `false`。
   症状：点被高亮的控件点不动、点遮罩空白反而退出。
   → 现在**同一份状态一个写者**（`updateMouseTransparency()`），两个标志取"或"。

2. **走过一个实操主题之后，"自动"再也不翻页了。** 曾经把 `auto_mode_` 直接置
   false（而不是按"用户请求的自动挡 && 主题不是实操式"算），后面全是讲解主题
   也不自动翻了。

3. **模式推送各走各的路。** 主题级 `enterFunc` 与模式推送原先只在"跨主题前进/
   回退"那条路上做，直启（`startOne`/`startAll`）那条漏——统一收口到
   `execute()` / `applyTopicOptions()`。

4. **"还有没有下一站"有两个出口。** `canPrev/canNext` 原先拿
   `topics().count()-1` 判"最后一条"，而数组末尾可能是**占位主题**
   （`stepCount()==0`，只在首页建卡、不进播放）：末尾是占位主题时最后一步永远
   显示「下一步」而不是「完成！」，首端是占位主题时「上一步」亮着却点不动。
   → 只留 `nextPlayableTopicIndex` / `previousPlayableTopicIndex` 两个谓词。

5. **回退主题只刷了几何、没刷内容**：演示层留着旧主题的标题与文案。

### 绘制与几何

6. **`QRect::center()` 是 `(x1+x2)/2` 向下取整**，不是几何中心（宽 200 的矩形
   400..599 中心报 499 而不是 500）。摆气泡用它，所以单测里凡是按"目标中线"
   手算的期望值都比几何中线小 1。第一版单测就是在这儿一次红了 6 条。

7. **多靶的洞必须 `WindingFill`。** 路径集合运算把路径当"区域"，区域由 fill
   rule 决定：默认 `OddEvenFill` 下两个重叠的圆角矩形在重叠区算"偶数次覆盖"、
   当洞外，相减之后那块仍被遮罩涂黑。相邻卡片间距 8px、而框往外扩 10px，必然
   重叠——不改就是一条条黑竖条。

8. **点空白退出 vs 点在靶上**：要**逐个矩形判、不合并并集**。多靶时把两块之间
   那块空白也算成"点在目标上"，遮罩就永远关不掉了。

9. **抢跑的高亮动画会覆盖这一帧的框**：连点两下「下一步」时上一步的动画还在跑
   （300 ms），它经 `valueChanged` 继续写"此刻显示的框"，把新值盖掉。所以换高亮
   时先 `stop()` 再写。

10. **气泡的换挂法与显隐躲不掉三种情况**：实操模式下气泡被提到独立顶层窗口
    （遮罩整棵子树对鼠标穿透，气泡留在里面就点不到；挂回宿主又会被遮罩压在
    下面）；讲解模式下遮罩 hide 时显式藏过气泡，而 **Qt 里被显式隐藏的子控件
    不会随父控件 `show()` 回来**（`showChildren` 会跳过 `isHidden()` 的子控件）
    ——第二遍就只剩遮罩。另外 `setParent()` 会把控件隐藏，换挂法之后必须显式
    恢复；`raise()` 不能省（遮罩每次 `setParent` 回 viewport 都插到最上层）。

11. **`TourCard` 的自绘盖住了 QSS。** 卡片一度同时自绘底色与边框，注释还写着
    "QSS 覆盖、无 QSS 时自绘兜底"——不成立：自绘是无条件的、且画在 QSS 之后。
    改 QSS 的人只会发现没反应。现在外观**只有 QSS 一个出处**，C++ 只做 QSS
    表达不了的按下-抬起配对。

12. **`TourStyle` 必须并入应用级样式表**，不能设在首页上：实操模式下气泡会被
    reparent 到 viewport，离开首页的子树，挂在首页上的样式盖不到它。

13. **`setHost(host, inflate)` 那个 `inflate` 是干什么的**：高亮目标**就是宿主
    窗口本身**时（比如整窗高亮），高亮框会等于整块遮罩，而遮罩是"整块挖掉高亮
    区"画的——阴影被整块掏空，屏幕上只剩气泡。往外扩一圈 `kHostMargin` 再把
    扩出去的部分按屏幕可用区裁掉即可（宿主已占满屏幕时自然扩不动）。

14. **锚点矩形原先在两个地方各算一遍**（`updateUi` 的控件分支与外部驱动的
    `showHighlight`）——两处实现漂移就会"同一步骤两次高亮框在不同位置"。现在
    共用 `RectInOverlay`。

15. **`clearHighlight()` 原先没停动画**：清完三个成员，在途动画下一帧又按旧值
    把框写回来——屏幕上留一个没有依托的孤框。

16. **气泡在实操模式下脱离了 Qt 的父子所有权**（被 `setParent(nullptr)` 提成
    顶层窗口），不显式回收就泄漏，而且那个窗口会一直留在屏幕上——析构里必须
    无条件删一次。

17. **`showHighlight*` 的 extras 早年漏赋值**：两步之间不一定有 `hideHighlight`，
    不"按参数无条件赋值"的话，上一步的多靶框会跟着留在屏幕上。

### 倒计时

13. **两种走法混在一个布尔里。** 自动挡（走到头 = 该翻页）与"外部驱动的视觉
    倒计时"（走到头**只收显示、不推进**）原先靠 `auto_mode_` 与
    `countdown_silent_` 两个布尔互相制约，散在三个函数里。现在收进
    `TourCountdown`：`finished()` 只在自动挡发，显示方从它读 `showing()`。

14. **起表那一帧必须自己刷状态**：不复位成"刚开始"（`time_percent_ = 0.0`、
    秒数最大），第一帧显示的还是上一次留下的空白，要等第一个 tick 才跳变。
    归零的是"**已过去**的比例"、不是剩余——给 1.0 会让进度条第一帧就是空的。

### 首页与菜单

15. **首页 `exec()` 原先等的是 `hideEvent` 里发的 `closed()`**：父窗口一藏、
    页面被 reparent、退出应用时子控件被隐藏，都会走到那条路——"用户选完了"
    与"页面没了"分不开，调用方会在用户根本没选的时刻继续往下走。现在只有
    `hideHomePage()` 发 `closeRequested()`（点卡片 / 全部 / 关闭都经过它）。

16. **菜单那条"模板导览"原先取 `topics().first()`**：菜单文字与"第几条"本是
    两回事，谁先注册谁就是——多注册一条主题就会播错。现在由宿主**显式传入**
    那条主题。标题文案也不许写死产品名（引导是通用库），默认取宿主
    `applicationName()`，需要时用 `setHeading()` 改。

### funq 桥接（对外契约）

17. **funq 是 CeCILL v2.1（copyleft）**，所以路径解析是照 `ObjectPath` 的行为
    自己重写的，不是拷源码——这是"零 funq 依赖"的由来。

18. **桥接必须是"可见但 0 尺寸"的独立顶层控件**：Windows 侧客户端会等控件
    visible 才返回，不 show 就一直超时；而脚本第一步常常跑在宿主主窗口还没
    创建的时候（登录框上），所以不能挂在哪个窗口下面。

19. **不可见控件的几何是未初始化的**：从没显示过的 `QMenu` 报出来的位置是
    (0,0)，框会画到屏幕角落。→ 不可见控件不给高亮。

20. **额外靶必须与锚点同窗口**：跨窗口的靶映到遮罩坐标系后落在遮罩之外——屏幕
    上看不见框，而接口照样回报成功。

21. **`path` 与 `paths` 同时给**：原先静默挑一个，脚本笔误要查半天。→ 按
    `paths` 走并给一条 `qWarning`。

22. **`highlightCount` 是"框了几处"唯一的自动判定出口**：`Q_PROPERTY` 必须
    是**静态**的——funq 的 `properties()` 走 `metaObject()->propertyCount()`，
    运行期 `setProperty` 建的动态属性读出来是 `KeyError`，不是报错。

23. **寻址锚点不能改名**（用例与脚本按路径/名字寻址）：
    - objectName `tourOverlay`、气泡 `bubble_self`（父子关系也不能变，用例路径
      是 `tourOverlay::bubble_self::bubble_text`）；
    - 桥接槽名 `showHighlight` / `showHighlightRect` / `hideHighlight` 等——
      `examples/funq-scripts/*.py` 按名字 `call_slot`。

### 样式与资源

24. **`ensureApplied()` 的两个 early return 一旦记下 applied，样式就永远不生效
    而且不报错**（QFile 打不开、样式为空、无声失败）。

25. **`Q_INIT_RESOURCE` 必须在全局作用域**：它展开后调的是 rcc 生成的全局
    函数，放进命名空间会去找不存在的 `ns::qInitResources_tour_res`。

26. **静态库里的 qrc 符号不会被自动链接进来**，得有代码显式引用一次
    （`tour_style.cpp` 就是那个引用点）。

27. **首页 `initStyleSheet()` 曾经自己又设一份样式**，与 `TourStyle` 并入应用级
    的那份打架、同一份 qss 读两遍。

---

## 三、留给后来人的两条硬约束

1. **改了库的公开接口，同步改 `docs/`**：这份记录里每一条都对应代码里一句
   "为什么"，改掉行为却留着注释比没有注释更坏。
2. **提交前过 clang-format**（CLAUDE.md 第 20 条）；`.clang-tidy` 里的命名约定
   只固化全仓一致的那几类，方法名靠评审。
