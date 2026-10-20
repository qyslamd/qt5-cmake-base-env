# Markdown 渲染重排去抖设计

状态：v5（2026-10-19），步 1 + 步 2 已实现并通过测试；步 3（宽度变化去抖，
预留补丁 0009）未做，等实测后再定。实施记录见文末 §八。0005 已被打印单位
修复占用，0007 由图片懒加载预留、0008 由 QPixmap 直供占用，见
`docs/guide/第三方依赖补丁.md`。

## 一、问题

`MarkdownView` 里的文档区是 `QLiteHtmlWidget`。窗口每变一次尺寸——拖分栏、拉窗口、
Ctrl+滚轮缩放——它都会把整篇文档重新布局一次。demo 侧实测布局约 **90ms/KB**
（54KB 文档一次实测约 3s），拖动期间每一帧都可能等一次全量布局。

现在的行为（`3rdparty/qlitehtml-180e3eb/src/qlitehtmlwidget.cpp`）：

```cpp
void QLiteHtmlWidget::resizeEvent(QResizeEvent *event)   // :630
{
    withFixedTextPosition([this, event] {
        QAbstractScrollArea::resizeEvent(event);
        render();                                        // 无条件全量重排
    });
}

void QLiteHtmlWidget::render()                           // :770
{
    if (!d->documentContainer.hasDocument()) return;      // 唯一的空文档守卫
    const int fullWidth = width() / d->zoomFactor;
    const int w = fullWidth - scrollbarWidth - 2;
    d->documentContainer.render(w, vViewportSize.height());   // 贵
    // 之后按 documentWidth/Height 更新四个滚动条参数 + viewport()->update()
}

void QLiteHtmlWidget::setZoomFactor(qreal scale)         // :483
{
    d->zoomFactor = scale;
    withFixedTextPosition([this] { render(); });          // 每一档滚轮排一次
}
```

## 二、布局输入到底是什么

第一轮审查推翻了一个想当然的前提，这里单独说清。

`DocumentContainer::render(width, height)`（`container_qpainter.cpp:1185`）做的是：
写 `m_clientRect = {0,0,width,height}`、清动画矩形、`if(!m_document) return`、
`m_document->render(width)`、`updateSelection()`。而 litehtml 一侧：

- `document::render(max_width)` 会把 `get_viewport().height` 填进 `cb_context.height`
  （`litehtml-0.10/src/document.cpp:503-509`），**百分比高度、百分比 min/max-height
  都会读它**；
- `position: fixed` 的几何也在 `render_positioned()` 里按 viewport 算
  （`render_item.cpp:243-278`）；
- `vh` / `vw` 乘的是 `m_media`，而 qlitehtml 的 `get_media_features` 只设了
  `type`（`container_qpainter.cpp:1128-1132`），media 宽高默认 0
  （`types.h:989-999`）——**`vh` 在这套组合里恒等于 0**。

结论：**布局输入是"宽度 + 视口高度"，只是因为当前 CSS 不含百分比高度与 fixed，
高度这一维才恰好无效。** 用"CSS 没有 vh"当判据是查了个永远不生效的变量。

而这两个来源都能进文档：md4c 放行原始 HTML（`markdown_to_html.cpp:381`、
`:513-514` 的 `MD_DIALECT_GITHUB`），`MD_MARKDOWN_CSS` 还能整体换掉样式表
（`markdown_view.cpp:75`、`:96-121`）。所以前提必须写成一条**可校验的不变式**，
而不是"litehtml 只吃宽度"。

### 不变式与守卫

> 当且仅当"最终 HTML + 生效 CSS"里不含百分比高度与 `position: fixed` 时，
> 纯高度变化可以跳过布局。

守卫放在**补丁的 `QLiteHtmlWidget::setHtml()`** 里，不放在 `MarkdownView`：那里
content 在手、CSS 已内联，`ApplyDocument`、`ShowMessage`（错误页同样走
`MD_MARKDOWN_CSS`）、上游 `setAntialias` 内部的 `setHtml` 全都经过它，一次覆盖、
没有绕过的入口；而且每次 `setHtml` 都重算，天然没有"标志粘滞"的问题。

```cpp
// 补丁内，setHtml() 开头
d->heightAffectsLayout = ScanViewportDependent(content);
```

扫描规则（都按 CaseInsensitive）：

1. **先剥掉代码形态再扫**：`<pre …>…</pre>`、行号模式的
   `<table class="md-code">…</table>`、行内 `<code …>…</code>`（本仓渲染器
   三种形态都会产出，见 `markdown_to_html.cpp:413-424`）。代码里出现
   `height:100%` 是文档在讲 CSS，不是真的在用它；不剥会恒真；
2. **剥 CSS 注释** `/\*.*?\*/` 与 HTML 注释 `<!--.*?-->`：litehtml 的
   tokenizer 会吞掉注释，`height:/*x*/50%` 是真声明，不剥会漏检；
3. 命中 `(?<![\w-])(?:min-|max-)?height\s*[:=]\s*["']?\s*[0-9.]+%`——`:height` 与
   `height=` 都要（`html_tag.cpp:1575-1587`、`el_image.cpp:29-31`）。
   开头的负向断言必须有：没有它，`line-height: 150%` 会因词边界命中；
4. 命中 `(?<![\w-])(?:top|bottom)\s*:\s*[+-]?[0-9.]+%`：视口高度会传给根
   containing block（`document.cpp:503-509`、`render_item.cpp:1324`），
   relative/absolute 的百分比上下偏移随视口高度变化（`render_item.cpp:116-137`）。
   左右方向按宽度解析，不受影响；
5. 命中 `var\s*\(`（保守）：litehtml 0.10 支持自定义属性替换
   （`style.cpp:143-152`、`:1657-1670`），`height: var(--h)` 配 `--h: 50%`
   是真实的百分比高度，正则扫不到声明值。只要出现 `var(` 就退回全量；
6. 命中 `position\s*:\s*fixed`；
7. `calc()` 不用管：litehtml 0.10 的 `to_pixels` 不支持它；`vh` 也不用管（恒 0）。

扫描是线性字符串匹配，54KB 的文档在 µs 级。命中只影响 `resizeEvent`，
`setHtml`/`setZoomFactor`/`setDefaultFont` 本来就走全量，不受影响。

## 三、方案

### 方案 A：给 qlitehtml 打补丁（推荐）

补丁编号 **0004**，文件名 `patches/qlitehtml/0004-layout-on-width-change.patch`。
图片懒加载设计原本也占 0004，现顺延为 0007（见那份文档 §四 开头）。两条补丁都会
改 `qlitehtmlwidget.h/.cpp`（0004 加布局接口，0007 加图片替换接口），**后做的必须
在已打前一条的树上重出 diff**；与 0001~0003 无文本 hunk 重叠（第二轮审查已核）。

补丁改两个文件：

| 文件 | 改动 |
|---|---|
| `src/qlitehtmlwidget.h` | 新增 `layoutWidth()`、`updateScrollBars()`、`setZoomFactorWithoutRender(qreal)`，以及两个 `Q_INVOKABLE`：`layoutCount()`、`documentHeight()`（消费者见 §五） |
| `src/qlitehtmlwidget.cpp` | `resizeEvent` 分叉、`lastLayoutWidth` 记账、`setHtml` 里的扫描与复位、`layoutCount` 自增 |

`resizeEvent` 的形态：

```cpp
void QLiteHtmlWidget::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);          // 视口必须始终跟随
    const int w = layoutWidth();
    if (w <= 0 || d->heightAffectsLayout || !d->documentContainer.hasDocument()) {
        withFixedTextPosition([this] { render(); });  // 完整路径才包位置保持
        return;
    }
    if (w == d->lastLayoutWidth) {
        updateScrollBars();
    } else {
        withFixedTextPosition([this] { render(); });
    }
}
```

要点，都是审查里点出来的：

- `lastLayoutWidth` 初值 **-1**（不是 0）：宽度算出来可能是 16 这类小值，用 0
  当哨兵会和"未布局"混淆；
- `updateScrollBars()` 自己判 `hasDocument()`：`documentWidth()/documentHeight()`
  没有空指针检查（`container_qpainter.cpp:1204-1212`），现在靠 `render()` 开头那句
  守卫兜，便宜路径绕过了它；
- `updateScrollBars()` 末尾保留 `viewport()->update()`，高度变化后的重绘不能只靠
  resize 自带的 paint；
- **便宜分支不包 `withFixedTextPosition`**：它找的是"第一个 y >= 视口顶的元素"
  （`container_qpainter.cpp:1601-1613`），没重排时 y 与动作前相同，包了只会把滚动
  对齐到块首、再被新 `maximum` 钳一次，纯副作用；
- **`render()` 与 `resizeEvent` 共用 `layoutWidth()`**：宽度计算（整数截断顺序、
  减滚动条与边框）必须只有一处实现，否则便宜分支与全量路径会差一像素；
- `lastLayoutWidth` 在 `documentContainer.render()` 成功返回后立刻写，避免
  `updateScrollBars()` 触发滚动条显隐、嵌套 resize 时缓存还是旧值；
- `setHtml()` 在 `setDocument()` 前把 `lastLayoutWidth` 置 -1（文档为空/解析失败时
  不能误走便宜分支）；
- **不做 `layoutInProgress_` 早退守卫**：嵌套 resize 时早退会丢掉宽度变化，外层
  结束后仍按旧宽度布局；今天 `setHtml` 同步、资源回调不跑事件循环，不存在重入。
  将来真出现重入，正确做法是"置 renderPending_、外层返回前重跑"，不是 if 早退。

**不做 `updateClientRect`**：`m_clientRect` 唯一的读者是 `get_viewport()`
（`container_qpainter.cpp:1113`），而它的读者全在全量布局路径上（`document::render`、
`render_positioned`、fixed 元素的 `get_placement`），下一次
`DocumentContainer::render` 又无条件重写它——便宜分支写一份没人读的值纯属白改，
还平白多一处与图片懒加载补丁的重叠。fixed 元素本来也修不了：它的几何是
`render_positioned` 算的，真要重定位得走 `render_fixed_only`
（`document.cpp:511-514`）。

### 方案 B：`MarkdownView` 里子类化 `QLiteHtmlWidget`

不推荐，但理由不是"做不到"（第一轮审查指出原写法是假二分）：子类可以显式调
`QAbstractScrollArea::resizeEvent`，也能靠 `rangeChanged` 反推文档高度。问题是
要**复制上游的滚动条算法与文档尺寸缓存**，`setHtml` 又不是虚函数、拦截不完整，
上游改一次就得跟一次。成本与漂移风险都比补丁高，收益一样。

### 方案 C：事件过滤器吞掉 resize

否。`QAbstractScrollArea::resizeEvent` 负责让 viewport 跟着变尺寸，吞掉它视口就
停在旧尺寸。这是控件的基本行为，不能拿去当节流点。

## 四、决策：分三步落地

| 步 | 内容 | 落点 | 依赖 |
|---|---|---|---|
| 1 | 缩放合并：连续滚轮/按键只在停手后排最后一档 | 本仓 `MarkdownView` | 步 2 的 `setZoomFactorWithoutRender()` |
| 2 | 宽度判断：宽度没变不重排 | 补丁 `qlitehtml/0004` | 无 |
| 3 | 宽度变化去抖：拖动期间不排，停手 150~250ms 补一次 | **单独的** 补丁 `0009` | 步 2 |

补丁是整体应用的，**debounce 必须独立成 0009**，否则步 2 没法单独验收。步 3 默认
不做，等步 2 实测。

要认清收益面：步 2 只救"宽度不变、只变高度"——拉窗口下边缘、md_viewer 用 Ctrl+F
显示/隐藏顶部查找栏；**横向拉窗口、拖竖向分栏、折叠右侧 dock（隐藏后中央区变宽）
每帧变的都是宽度，一步都省不了**——那是步 3 的活。别把步 2 当成"拖分栏不卡"。

### 步 1：缩放合并

全仓只有 `markdown_view.cpp:324` 一处调 `html_->setZoomFactor`，且 `SetZoom` 是
private、只有快捷键与 Ctrl+滚轮两个入口，合并放在 `SetZoom` 里覆盖全部现状。
规则：

- `zoom_` 立刻更新（对外状态一致，连按不丢档）；`html_->setZoomFactor` 交给
  single-shot 定时器（150ms 常量）合并；
- 定时器回调先 `qFuzzyCompare(html_->zoomFactor(), zoom_)` 再调——`setZoomFactor`
  自己不判等，设回原值也会白排一次；
- `InvalidateAnchorYCache()` 在**应用**时调；
- **不做"换文档时作废挂起缩放"**：只丢定时器会让 `zoom_` 与控件永久分叉，而
  `SetZoom` 开头就拿 `zoom_` 比较，之后再设回同一个值会直接早退。规则改成：
  `ApplyDocument()` 里 `html_->setHtml(final_html)` 的**前一行**停掉定时器并调
  `setZoomFactorWithoutRender(zoom_)`——补丁提供的这个入口只改 `d->zoomFactor`、
  不触发 `render()`，所以零额外布局；`setHtml` 随后按新缩放排一次。这两步之间
  **不得 `processEvents()`/`repaint()`**，否则会画到"新缩放 × 旧换行"的一帧；
- 为了可测（§五），本仓新增公开 `SetZoomFactor(qreal)` 与 `zoom()`：只转发
  `SetZoom`，别再开第二个 `html_->setZoomFactor` 调用点。`todo.md` 里"缩放公开
  API + 示例控件"那条，本次只收 API 那半，示例控件另做；
- `ScrollToAnchor()` 开头 flush 挂起缩放：`scrollToAnchor` 用旧宽度下的 anchorY 并
  按旧 `maximum` 钳位（`qlitehtmlwidget.cpp:546-558`），重排后锚点会偏；
- `FindText()` 同样在空串早退之后、调用 `html_->findText` 之前 flush：
  未落地的缩放会让命中位置在 150ms 后漂移（`qlitehtmlwidget.cpp:497-524`）；
- `ShowMessage()` 走 `setHtml` 前也要停掉挂起定时器并
  `setZoomFactorWithoutRender(zoom_)`，否则错误页会先按旧缩放排一次、150ms 后
  再排一次；
- 析构不主动 flush（QTimer 以 `this` 为 context 会随之取消，主动 flush 只会白排一次）。

### 步 2：宽度判断

即上节补丁。注意 `setHtml` / `setZoomFactor` / `setDefaultFont` / `setAntialias`
仍走完整 `render()`（审查确认这些入口没有遗漏），只有 `resizeEvent` 分叉。

### 步 3：宽度变化去抖（0009，默认不做）

形态：宽度变了 → 记下待处理、重启定时器、先不排；到点 → 重新取 `layoutWidth()`
与 `lastLayoutWidth` 比，一样就返回，不一样才 `render()`；期间画旧布局（变宽留白、
变窄裁掉）。任一完整 `render()`（`setHtml`/`setZoomFactor`/`setDefaultFont`）都要
停掉挂起的 debounce，避免换文档后多排一次。

确定性结论（审查给的口径）：Qt 对同尺寸 `setGeometry` 不发 resize，single-shot
重启不会饿死；真正的坑是慢拖时每个事件都会排一次、以及 3s 的 render 期间排队的
resize 在返回后再走一遍（靠 `lastLayoutWidth` 让第二次走便宜路径）。

## 五、验证

判据必须是**布局次数**，不能是 `documentHeight()`——同宽不同高时后者本来就不变，
未打补丁的实现同样通过，那是假阳性测试。

1. **布局计数**：补丁加 `Q_INVOKABLE int layoutCount() const`（照 `html()` /
   `selectedText()` 已是 Q_INVOKABLE 的做法）。测试沿用
   `render_toggles_test.cpp:41-49` 的 `findChild<QWidget*>("markdownHtml")` +
   `QMetaObject::invokeMethod`：
   - 纯高度 resize → 计数不变；
   - 宽度 resize → 计数 +1；
   - `setHtml` → 计数 +1；
   - 守卫用例逐条覆盖：`height:50%`、`height:var(--h);--h:50%`、
     `position:relative;top:10%`、`position:fixed`、CSS 注释包着的高度
     （`height:/*x*/50%`）→ 纯高度 resize 均计数 +1；紧接着换成普通文档 →
     又变回 +0（证明每次 `setHtml` 都重算、不粘滞）；
   - 反例：行号模式表格与非行号 `<pre>` 里写 `height:100%`、行内 `<code>`
     里写同样内容 → 纯高度 resize 计数 +0（代码形态被剥掉）。
2. **缩放合并的判据**：`setZoomFactor` 不是 slot，`SetZoom` 又是 private，测试摸不到。
   所以本仓新增公开 `MarkdownView::SetZoomFactor(qreal)` 与 `zoom()`（见 §四 步 1），
   测试走它们：150ms 内连调三次 → `layoutCount` 只 +1、`zoom()` 停在最后一档；
   再补一条"三次调用间隔均 >150ms 且每次 processEvents → +3"，证明合并有窗口
   而不是恒等。计时用 `QElapsedTimer` + 循环 `processEvents()`，不引入 QTest；
   基线前先等掉 `SetContent` 可能残留的定时器。
2b. **挂起缩放 + 换文档**：`SetZoomFactor` 后立即 `SetContent`（或 `ApplyDocument`
   路径）→ 布局计数只 +1（来自 setHtml），150ms 后不再增加，验证
   `setZoomFactorWithoutRender` 的零额外布局规则。
3. **`documentHeight()` 的消费者**：补丁加 `Q_INVOKABLE int documentHeight() const`
   （带 `hasDocument` 守卫）。测试用它断言：宽度 400→800 后高度变小/变化；
   纯高度 resize 后数值不变。这同时是"便宜分支没改变文档尺寸"的独立证据。
   qlitehtml 在 `src/markdown/CMakeLists.txt` 是 PRIVATE，测试只能走
   `QMetaObject::invokeMethod`。
4. **测试环境**：先 `resize` 再 `show()` 再 `processEvents()`，并且在 `SetContent`
   之前定好尺寸——否则首次 render 发生在默认 100×30，第一次 resize 会被记成宽度
   变化。宽度用 **400→800**（都小于 `body{max-width:960px}`），文档高度远大于视口，
   避开纵向滚动条显隐的边界噪声。offscreen 可能没有字体族
   （`mermaid_renderer_test.cpp:25-28`），所以宽度敏感性只断言布局计数，不断言
   `documentHeight` 的差值。
5. **像素/真实平台**：走 demo_app + funq（`scripts/funq/probe_markdown_view.py`
   抓的是 demo_app 的 `markdownWindow::markdownView`；`md_viewer` 主窗口没有
   objectName、也没有 runner）。先窄后宽各抓一张，确认文字确实按新宽度重排。
   宽度选择的参照是 `.md-page{max-width:980px}`（2026-10-19 版面调整后的
   页容器，旧方案的 `body{max-width:960px}` 已撤销）。
6. **手工**：拉窗口下边缘、md_viewer Ctrl+F 开合查找栏（步 2）、横向拉窗口（步 3）、
   Ctrl+滚轮连按、Ctrl+0 复位。
7. 构建与测试口径：`scripts/build_ninja.bat -t relwithdebinfo -a x86` +
   `ctest -L MARKDOWN`。

## 六、风险

| 风险 | 影响 | 处理 |
|---|---|---|
| 文档/覆盖 CSS 含百分比高度或 fixed | 跳过布局，显示按旧视口高度算的结果 | §二 的扫描守卫，命中即退回全量；每次 `setHtml` 重算，不粘滞 |
| 守卫误判（代码块里的示例代码、`line-height:150%`） | 优化对整篇文档失效（不会错排） | 先剥 `<pre>`、`(?:min-\|max-)?height` 锚定、大小写不敏感 |
| 空文档 / 宽度 0 / 最小化 | 空指针或负宽度 | `lastLayoutWidth=-1`、`w<=0` 退回全量（与上游现状一致）、`updateScrollBars` 判 `hasDocument` |
| `AnchorY` 缓存被反复打掉 | 纯高度 resize 后下一次查询要重填整表（O(标题数 × DOM 搜索)） | 已纳入本次范围，见 §七.6：只在 `layoutCount` 变化时失效 |
| 去抖期间命中/锚点/滚动条范围是旧值 | 150~250ms 内可能点错链接、滚不到目标 | 步 3 的风险，实测后再定 |
| QSS 改滚动条宽度（`PM_ScrollBarExtent`） | 缓存宽度对不上，优化失效 | 退化为一次全量重排，不是错误 |
| `setHtml` 期间嵌套 resize | 走错分支 | `layoutInProgress_` 守卫 |
| 补丁没生效 | 编译失败（本仓会调 `layoutCount`/`setZoomFactorWithoutRender`）或 configure FATAL_ERROR | 响亮失败，不是静默退化；在 `cmake/patch_dependencies.cmake` 注册，改补丁后删 `.patch_applied_0004-*` 重放 |

DPR 变化是否触发 resize 未验证；即便触发，布局输入是逻辑像素，代价只多一次全量
render，不新增正确性风险。

## 七、已定决策（2026-10-19）

1. **高度守卫自动做，且落在补丁的 `setHtml()` 里**：每次 `setHtml` 都重算
   `heightAffectsLayout`（扫最终 HTML，CSS 已内联），命中百分比高度或
   `position: fixed` 就退回全量重排。不放在 `MarkdownView`：`ShowMessage`、
   `setAntialias` 也走 `setHtml`，放上面才能一次覆盖；每次重算才能避免"标志粘滞"。
   扫描先剥 `<pre>` 再匹配，避免把讲 CSS 的代码块当成真用法。
2. **换文档的缩放落地用补丁新入口** `setZoomFactorWithoutRender()`：只改
   `d->zoomFactor`、不排。`setZoomFactor()` 保持现有语义不动。
3. **步 3 本次不做**：先落地步 1 + 步 2，量一遍步 2 在"只变高度"路径上的收益，
   再决定 0009 要不要写。
4. **砍掉 `updateClientRect`**：它在便宜分支上写进去的值没有读者，下次全量 render
   又会覆盖；补丁因此只改 `qlitehtmlwidget.h/.cpp` 两个文件，也少一处与图片懒加载
   补丁的重叠。
5. **为了验收，本仓新增公开的 `SetZoomFactor(qreal)` / `zoom()`**：`setZoomFactor`
   不是 slot、`SetZoom` 又是 private，没有公开入口连"连续缩放只排一次"都断言不了。
   公开 API 只转发 `SetZoom`，不开第二个调用点。

## 八、实施记录（2026-10-19）

- **补丁 0004**：`patches/qlitehtml/0004-layout-on-width-change.patch`，在
  `cmake/patch_dependencies.cmake` 注册并重放验证。`QLiteHtmlWidget` 新增
  `layoutCount()` / `documentHeight()` / `setZoomFactorWithoutRender()`；
  `resizeEvent` 在宽度未变且守卫未命中时只刷新滚动条；`setHtml` 每次重算
  守卫并复位 `lastLayoutWidth`。
- **守卫**按 §二 的 7 条规则实现（剥 `<pre>` / 行号表格 / 行内代码 /
  CSS/HTML 注释；百分比高度、top/bottom 百分比、`var(`、fixed）。
- **MarkdownView**：`SetZoomFactor()` / `zoom()` 公开；缩放经 150ms 单发定时器
  合并，`ApplyDocument`/`ShowMessage` 用 `setZoomFactorWithoutRender` 落地，
  `ScrollToAnchor`/`FindText` 前 flush；锚点缓存改为查询时比较 `layoutCount`，
  删除 `resizeEvent` 与 `rangeChanged` 两套无条件失效。
- **测试**：`tests/markdown/layout_debounce_test.cpp`，8 条用例覆盖纯高度不重排、
  宽度重排、五类守卫、三种代码形态、连续/间隔缩放、挂起缩放 + 换文档。
- **验证**：x86 relwithdebinfo Build OK；`ctest -L MARKDOWN` 17/17。
- **未做**：步 3 的宽度变化去抖（补丁 0009）；`md_viewer` 缩放已交付
  （工具栏 + 状态栏百分比），不再是遗留项
  （公开 API 已有，UI 仍在 todo）。
6. **锚点缓存改为查询时按 `layoutCount` 判新鲜度（2026-10-19 并入，复审修订）**：
   现有 `rangeChanged` 无条件失效 + `MarkdownView::resizeEvent` 无条件失效两条线
   都会在纯高度 resize 时打掉缓存；而只比较 rangeChanged 也不够（布局变了但滚动
   范围没变时不发信号，且计数自增早于 handler 会读到旧值）。最终方案：
   - `AnchorY()` 入口比较 `html_->layoutCount()` 与上次填充缓存时记下的计数，
     不同才整表重填；不依赖任何信号时序；
   - 删除 `MarkdownView::resizeEvent` 里的 `InvalidateAnchorYCache()`；
   - `rangeChanged` 的接线可以保留（廉价提示）也可以删，本设计选择删除，避免
     两套失效机制并存；
   - `ApplyDocument`/换文档仍显式失效（文档对象整个换了，计数未必立刻变）；
   - 已知既有问题不要依赖：`anchorY` 对"找到但尚未布局"的标题返回 0 而非 -1
     （`container_qpainter.cpp:1214-1236`），"全都没查到保持 dirty"分支实际不生效。

