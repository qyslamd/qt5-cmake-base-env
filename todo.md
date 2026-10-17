# 待办

记录**已交付但还没做完**的事，做完一条划一条。按"什么时候会疼"排序，不按新旧。

## 渲染能力扩展后续（2026-10-17）

- [ ] **图片懒加载阶段 2**：方案见 `docs/plan/Markdown图片懒加载设计.md`（讨论中）。
      依赖 qlitehtml 补丁 0004（`setImageSizeHandler` + `replaceImage`）；首屏张数/
      字节预算、是否并入图片字节 LRU 仍待拍板。
- [ ] **图片字节 LRU（原阶段 1，暂缓）**：单独做收益低（只省重复读盘/解码）；
      若做懒加载，建议作为重渲染配套并入阶段 2。
- [ ] **缩放公开 API + md_viewer 控件**：`MarkdownView` 目前只有私有 `SetZoom`，
      界面只靠 Ctrl+滚轮 / Ctrl+=/-。要在示例里展示缩放，需补
      `zoomFactor()` / `SetZoomFactor()`，再加滑块或百分比标签。

## 渲染性能（设计与硬约束见 `docs/plan/mermaid图表渲染设计.md` §7.2）

- [ ] **把公式与图表的渲染搬进专用 worker 线程。** 现在两者都在 UI 线程同步跑：
      首次求值 3.5MB 的 mermaid bundle 是秒级、MathJax 1.6MB 也要一两秒，一篇带
      若干图的文档会让界面连续卡住。搬线程时的硬约束（别重新踩）：
  - `JS_NewRuntime` / `JS_UpdateStackTop` 必须在渲染线程执行（栈检查基址按**创建
    线程**记录），UI 线程建、渲染线程用 = 配额形同虚设；
  - **JS 配额 ≤ worker 线程栈**（现在是 16MB 配额 ≤ 宿主 32MB `/STACK`）；Linux 的
    `QThread` 默认只有 8MB 栈，要显式 `setStackSize`（只在 Linux 生效）；
  - 预热要在线程起来之后 post 任务，别放构造函数里——窗口是懒建的、构造发生在
    UI 线程，"预热不占 UI 时间"就不成立；
  - 要"先出占位、结果回来再 setHtml"的话：得带 generation 号丢弃过期结果，并在
    第二次 `setHtml` 之后恢复滚动位置（`markdown_view.cpp` 现在在
    `ApplyDocument()` 前后存/恢复滚动值）；
  - 动手前先定死一件事：`MarkdownToHtml` **整体**搬到渲染线程跑（sink 保持同步），
    还是把 sink 异步化（接口要扩成带批次的，公式侧同改）。**二选一，别混。**
- [ ] 载荷压缩复量：`markdown_res.qrc` 的 `<qresource>` 可以加 `compress`（zlib）。
      现在 `qrc_markdown_res.cpp` 已约 14MB，编译时间与 exe 体积值得量一次再决定。
- [ ] **resize 会触发整篇重排（拖动分栏/拉窗口时每帧一次）。** `qlitehtmlwidget.cpp`
      的 `resizeEvent` 直接调 `render()`，而 `render()` = 整篇 litehtml 布局
      （`documentContainer.render(w, h)`）。demo 侧实测布局约 **90ms/KB**：54KB
      文档一次 `setHtml` ≈ 3s，所以拖分栏的每一帧都可能等一次全量布局；窗口
      最大化、拉伸同理。`setZoomFactor` 也是一个道理（Ctrl+滚轮每档排一次），
      `paintEvent` 只 draw、滚动不受影响（这条别改错地方）。修法（都在本仓侧，
      不必等上游）：
  - resize 去抖：拖动期间不重排（旧内容缩放显示或留白），停手 150~250ms 补一次；
  - **宽度没变就不重排**：纯高度变化不影响 litehtml 布局（本仓 CSS 不用
    `vh`/`position:fixed`）——这条是一行判断，收益最大；
  - 缩放同样合并（连续滚轮只排最后一档）。
  qlitehtml 的解压树不入库（`3rdparty/*/` 被 ignore），改动走
  `patches/qlitehtml/`（见 `docs/guide/第三方依赖补丁.md`）；也可以子类化只在
  `MarkdownView` 里用。demo 与基座是同一份控件，两边一起受益。

## 已知偏差（设计 §7.1，暂时没咬到）

- [x] ~~CSS 层叠与规范不一致~~（2026-10-17 已修：规则按 specificity 决胜、
      内联 style 高于规则 normal、只有规则 `!important` 能压内联；测试钉在
      `mermaid_svg_cleanup_test` 的 `MermaidCssInlinerTest` 三条）
- [ ] `d` 属性方向：拿"最后两个坐标对"当方向，小写相对命令（`m`/`l`/`c`）与 `A`
      弧命令会算飞。flowchart classic 边是绝对 `M`/`L`/`C` 恰好没踩到，换图类型要补。
- [ ] `journey` / `timeline` 要补 `textPlacement: 'tspan'`，否则这两类图走
      `<foreignObject>`，图上文字全丢（现在没覆盖这两类）。

## 验证面

- [ ] **Linux / WSL 侧没验**（本轮口径只有 Windows x86 relwithdebinfo）：系统 Qt
      5.12 下的字体族探测链（`Noto Sans CJK SC` / `Source Han Sans SC` / 文泉驿）
      是纸面推理，没在真机跑过；`dist` 路径推导也是照 demo 的经验写的。
- [ ] **像素验收还没落成基座的脚本**：这次的量化判据（标签相对节点框心的偏移、
      消息线末端箭头暗像素数）目前只活在 demo 的 `temp/measure_render.py` 里。
      `scripts/funq/` 下应该有一个同款探针，发布前跑一遍。
- [ ] offscreen 平台**没有字体库**（实测 `QFontDatabase::families()` 返回 0 个族）：
      端到端单测只能断言"图出来了、是 well-formed 的 `<img>`"，文字像素养不了。
      像素验收必须走真实平台（App + funq 抓图），这条已写进测试文件头注释。

## 文档 / 杂项

- [ ] 发布前按 release 重量体积表：`docs/guide/发布指南.md` 的 Windows/Linux 数字
      是上一版发行时的；本轮只量了 x86 relwithdebinfo（exe 9.3MB、qrc 生成源 13.9MB）。
- [ ] 载荷升级演练：`src/markdown/resources/extract_mermaid_payload.py --check` 的
      用法只写在配方注释里，升级 mermaidx 时按它走一遍，顺带确认 `.gitattributes`
      的 `-text` 让两个平台 checkout 出同样的字节（哈希才可比）。
- [x] ~~mermaid 的 `theme` 固定 `default`~~（2026-10-17 已随三套主题接入，
      `InlineSvgCss` 的层叠测试仍在 `mermaid_svg_cleanup_test`）

## 暗色主题（2026-10-17 已落地，保留当时的推理备查）

三套主题 CSS、公式前景色、Mermaid 主题均已接入 `MarkdownView::SetTheme`
（提交 `41b7506`）；下面三处"光栅化烧死"的判断是当时的依据：

- [x] 公式：`currentColor` 在光栅化前替换为具体色值，颜色进缓存 key
- [x] mermaid：主题走逐图 `%%{init}%%` 指令，themeVariables 进缓存 key
- [x] 文档里引用的位图图片不受控（预期内，不做）
- [x] CSS：三份主题样式表 + `MarkdownView::SetTheme()`；`MD_MARKDOWN_CSS`
      仍是最高优先级，覆盖时只切公式/Mermaid 配色并打警告
- [x] 控件外壳（滚动条、底色、按钮）由 Fluent/QSS 体系负责，不在文档 CSS 内

## 审查留档（2026-10-15，一轮外部 review）

那一轮提出的小改动已全部收掉（见 `git log`；`UnescapeHtml` 补 `&apos;` 那条的
细节——代码块里实体按 GFM 保持字面量，所以它防的是"字面单引号被漏成实体"——
记在 `markdown_to_html_test.cpp` 的用例注释里）。这里只留"查过、确认不用动"的
结论，免得下次重复怀疑：

- `AttrOf` 的 `\b%1\s*=\s*"…"`：`\brefX` 不会命中 `myrefX`，`refX` 也不会命中 `refY` ✓
- `InlineSvgCss` 给每个元素（含 `<svg>` / `<defs>` / `<marker>`）都写一份 `font-family`：
  SVG 体积涨一点，换来 QSvgRenderer 的确定性，可接受
- `MarkdownWindow::OpenFile` 的 `show/raise/activateWindow`：内链跳转走 `ShowDocument`，
  不会抢焦点 ✓（将来若要"打开但不抢焦点"，再加 `bool activate = true` 参数）
- `MetricsFor` 在两边 family 都空时落到 Qt 默认字体：只在这个平台连一个 CJK 族都没有时
  发生，此时本来就该报警告（`PickCjkFamily` 已经会 warning）

架构备注：`MarkdownWindow`（一窗一篇）与 `MarkdownView`（一篇一视图）的划分对"分栏预览 +
编辑器"是友好的——届时 `MarkdownView::SetContent(markdown, base_dir)` 就够用，不必改结构。

## 文档迁移（已办完，2026-10-17）

demo 的两篇复刻教程已收编 `src/markdown/docs/`；"模块文档随模块走"这条口径
同样把 `JS载荷复刻指南.md` 从 `docs/guide/` 挪了进去（`第三方依赖补丁.md` 的
指向已同步）。demo 那边就此不再维护。