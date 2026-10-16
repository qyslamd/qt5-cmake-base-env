# 待办

记录**已交付但还没做完**的事，做完一条划一条。按"什么时候会疼"排序，不按新旧。

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

## 已知偏差（设计 §7.1，暂时没咬到）

- [ ] CSS 层叠与规范不一致：内联 `style=""` 本该压过非 `!important` 的作者规则，
      现在是被覆盖；同属性还按文档顺序而非 specificity 决胜。当前只有字体族一处
      强制（写在所有规则之后，安全），做别的改写就会撞上。
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
- [ ] mermaid 的 `theme` 目前固定 `default`：要跟随应用主题（深色/浅色）得把它
      接到配置上，并注意主题 CSS 一变，`InlineSvgCss` 的层叠偏差就会被放大。

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

可选的扩展点（不急）：

- [ ] 标题 anchor：`[x](#heading-name)` 这种 GitHub 风格锚点现在跳不了——md4c 不给标题加
      `id`，只有显式 `<a name>` 有效。要做就在 `HighlightCodeBlocks` 之后过一遍 HTML 给
      `<h1>`–`<h6>` 补 `id`。等真要跨文档锚点再做

架构备注：`MarkdownWindow`（一窗一篇）与 `MarkdownView`（一篇一视图）的划分对"分栏预览 +
编辑器"是友好的——届时 `MarkdownView::SetContent(markdown, base_dir)` 就够用，不必改结构。
