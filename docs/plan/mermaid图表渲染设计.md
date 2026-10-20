# Mermaid 图表渲染设计

## 一、问题陈述

markdown 预览已能把 `$…$` 交给 MathJax 出图（`formula_renderer`），但
` ```mermaid ` 围栏目前只当普通代码块高亮。要在基座里补上图表渲染，约束：

1. 不引 Node.js、不引 WebEngine、不起外部进程——与公式那条路同一套约束；
2. 不引入不可分发的资产（字体是重灾区）；
3. 可测试：不能只靠"看一眼窗口"。

demo（`D:\trae_workspace\qt-md-renderer-demo`）已跑通这条链路（Windows + WSL
实测），本设计是**把 demo 的结论搬进基座**，并解决 demo 没解决的两件事：
字体与渲染同源、资产与文档面。复刻细节与踩坑见 demo 的
`docs/quickjs-mermaid图表渲染复刻教程.md`。

**本轮范围**：同步实现（UI 线程），**验证口径只有 `scripts/build_ninja.bat -t
relwithdebinfo -a x86`**（32 位），WSL 侧本轮不验。线程化另列一笔（见 D4）。

## 二、现状回顾

### 2.1 技术栈与来源

| 件 | 来源 | 许可 | 形态 |
|---|---|---|---|
| `mermaid.js`（内含 mermaid 11.16.0） | mermaidx-0.9.5 `assets/` | MIT | 3.5MB bundle，入 qrc |
| `dom_shim.js` | 同上 | MIT | 假 DOM/SVG，入 qrc |
| `path_bbox.js` | `mermaidx/path_bbox.py` 的 `PATH_BBOX_JS` | MIT | 入 qrc |
| mermaidx-0.9.5 | `D:\software\aa-sources\`（pip 包，**不入 3rdparty**） | MIT | 只在重建载荷时用 |

三份载荷的重建配方与 md5 见 `src/markdown/resources/extract_mermaid_payload.py`，
本机参考目录 `D:\software\aa-sources\mermaidx-0.9.5`。**不入 `3rdparty/`**：
它不是构建依赖，只是资产生成源，入 `3rdparty` 会让人以为构建要它。

### 2.2 demo 已验证的结论（可直接搬）

- QuickJS 里依次 eval `path_bbox.js` → `dom_shim.js` → `mermaid.js`，三个 FFI
  回调回答文本度量；`mermaid.render()` 的 Promise 靠 `JS_ExecutePendingJob` 泵
  （`dom_shim` 把 rAF/setTimeout 换成了微任务，循环能收敛）。
- QSvgRenderer 画不了 mermaid 的裸 SVG，需要三层后处理且**顺序固定**：
  `cleanSvg`（空 rect / dasharray / **把 tspan 位置烘焙回 `<text>`**）→
  `inlineSvgCss` → `inlineSvgMarkers`。
- 实测覆盖 flowchart / sequence / class / state / pie；Windows 与 WSL 布局一致，
  量化判据：标签相对节点框心 ±3 内、消息线末端箭头暗像素 > 0。

### 2.3 基座现状

- `src/markdown/` 静态库：`MarkdownToHtml(md, FormulaSink)`，代码块走
  `HighlightCodeBlocks()`；`formula_renderer` 是"qrc 载荷 + 懒加载 + 按内容缓存
  + 失败返回空串"的样板，`JS_SetMaxStackSize` 用 **16MB**。
- 全仓没有任何线程；`/STACK:33554432` 已通过 `target_link_options(INTERFACE)`
  传给宿主；`cmake/test_helpers.cmake` 的 `TEST_DEFAULT_TIMEOUT` 是 **10 秒**。
- 没有任何字体资产入库，也没有 `addApplicationFont` 调用。

## 三、决策记录

| # | 决策 | 理由 |
|---|---|---|
| D1 | 三份载荷入 qrc | 纯 JS、平台无关、部署不用跟文件；与 MathJax 同款。代价是 exe 与前缀编译量（见 §6.3） |
| D2 | **系统字体 + Qt 度量**（用户选定） | 不打包字体（许可风险与 ~10MB 资产一起消失）；度量与绘制都由 Qt 查同一份字体，量画同源。见 §4.4 |
| D3 | **两个载荷各持一个 JSRuntime**，不共用 | `dom_shim.js` 会覆盖 `globalThis` 上的 `console`/`CSS`/`URL`/`crypto`/`btoa`/`structuredClone`/`Intl`（见 `resources/dom_shim.js` 开头几处），MathJax 是同 global 的另一个 bundle，共用等于让两份 shim 互相改环境——demo 从没走过这条路。代价是两份运行时内存，x86 上可接受 |
| D4 | **本次只做同步版**，线程化另列一笔（用户选定） | 每步可编译、可验证、可回滚。线程化的硬约束先记下（§7.2），别丢 |
| D5 | 接口用同步 `MermaidSink` | 与 `FormulaSink` 同款依赖倒置：markdown 模块不依赖渲染引擎，单测塞假实现；也避免了"同步 sink 塞不进 request id"的自相矛盾 |

## 四、方案（同步版）

### 4.1 文件与职责

| 文件 | 内容 |
|---|---|
| `src/markdown/mermaid_renderer.{h,cpp}` | QuickJS 运行时（自持）+ 三个回调 + render/pump + SVG→PNG + 缓存 |
| `src/markdown/mermaid_svg_cleanup.{h,cpp}` | 文本定标烘焙、marker 展开、CSS 规则内联 + **字体族强制**、空 rect / dasharray 修补 |
| `tests/markdown/test_mermaid_svg_cleanup.cpp` | 纯函数用例（几何/结构） |
| `tests/markdown/test_mermaid_renderer.cpp` | offscreen 端到端（与 `test_formula_renderer` 同款） |

对外只有两个函数，与 `formula_renderer.h` 对称：

```cpp
// mermaid 源码 → 可直接内联的 <img>（PNG data URI）；失败返回空串
QString MermaidToImgTag(const QString& code);
bool MermaidRendererReady();
```

markdown 侧新增 sink（**保持同步**，签名与 `FormulaSink` 并列）：

```cpp
using MermaidSink = std::function<QString(const QString& code)>;
QString MarkdownToHtml(const QString& markdown,
                       const FormulaSink& formula = {},
                       const MermaidSink& mermaid = {});
```

`HighlightCodeBlocks()` 里 `language-mermaid` 走 sink；**sink 拿到的是已经
`UnescapeHtml` 过、`&lt;`/`&gt;`/`&amp;`/`&quot;` 全部还原的源码**（与高亮路径
同一份 `code`，别把 `--&gt;` 喂给 mermaid）。返回空串则按原语言退回
`HighlightCode(code, "mermaid")`（未知语言分支）。

### 4.2 后处理三层（细节见教程 §5）

顺序不可颠倒，且**必须在同一处注释里写明为什么**：文本烘焙要在 CSS 内联前
（`text-anchor` 之类的规则要作用到新建的 `<text>` 上）；marker 展开要在 CSS
内联后（箭头颜色来自类规则，早读到的 `stroke` 是 `none`）。

### 4.3 initialize 的"不得丢"清单

`htmlLabels:false`（含 flowchart/class/state 各自那份）、`startOnLoad:false`、
`theme:'default'`、`er.useMaxWidth:true`。少一个 `htmlLabels` 就是"flowchart
里文字全丢"。

### 4.4 字体：怎么做到"量与画同源"

三步，缺一不可：

1. **选族名**：启动时探一次平台可用的带 CJK 的族名（Windows `Microsoft
   YaHei`；Linux 用 `QFontDatabase::families()` 找 `Noto Sans CJK SC` /
   `Source Han Sans SC` / `WenQuanYi` 系，都没有就退回默认族并**记一条
   warning**——不要静默）。
2. **度量用 `QFontMetricsF`，不用 `QRawFont`**：`QRawFont::advancesForGlyphs`
   不做字体替换，而绘制走的是 QFont 的替换链，两者会在缺字时对不上。
   `QFontMetricsF(QFont(family, size)).horizontalAdvance(text)` 与
   `ascent()/descent()` 走的是绘制同一条路。
3. **强制族名**：在 `inlineSvgCss` 的属性写回阶段，把 `font-family` 覆写成该
   族名，**每个元素都写**（包括烘焙出来的 `<text>`）。这样既不依赖"Qt 会不会
   从祖先继承 font-family"（未验证项），也不受 CSS 层叠顺序影响（我们知道这
   一层与规范有偏差，见 §7.1）。改写要处理三种形态：`&quot;` 实体已解码、
   多族列表（`"trebuchet ms",verdana,arial,sans-serif`）、带空格的族名
   （`Microsoft YaHei`，写成 attribute 时该不该带引号要对一次）。

**判据（实施第一步就做）**：同一段中文+拉丁文字，分别用"目标族名"和"一个不
存在的族名"生成最小 SVG，经 QSvgRenderer 光栅化后比 ink 宽度与基线；要求：

- 两个 SVG 的 ink 不同（证明族名确实影响了绘制，替换点有效）；
- 目标族那份与 `QFontMetricsF` 给出的宽高对齐到 ±1px（证明量画同源）。

demo 的 DejaVu 基准（`"Hello, world"` → 94.78125）在 D2 下不再是判据，由上面
两条替代。

### 4.5 调试转储

demo 那套分阶段转储（`dbg_N_{raw,clean,inlined,marked}.svg` + `dbg_N.png`）是
定位问题的唯一有效手段，但它往 `applicationDirPath()` 写文件——**入库版本默认
关闭**，用环境变量（如 `MD_MERMAID_DUMP=<dir>`）打开，且不落在 exe 旁。

### 4.6 移植即改写（不是原样搬）

| 项 | demo | 基座 |
|---|---|---|
| 头文件保护 | `#pragma once` | 宏保护 `FILE_MARKDOWN_XXX_H_` |
| 命名 | `cleanSvg`/`inlineSvgCss` 等 camelCase | `CleanSvg`/`InlineSvgCss`/`MermaidToImgTag` 等 PascalCase |
| 缩进 | 4 空格 | 2 空格，过 `scripts/clang_format.bat` |
| `QString::SkipEmptyParts` | 同 | **保持 `QString::SkipEmptyParts`**：基座要跑 Qt 5.12（commit 4b685d0 刚修过同类问题），别"顺手升级"成 `Qt::SkipEmptyParts` |
| `JS_SetMaxStackSize` | 64MB | **16MB**（与 `formula_renderer` 一致，且必须 ≤ 宿主 32MB 线程栈，见 §7.2） |
| 注释稠密度 | 教程式长注释 | 只写意图，不引 docs（基座规矩 20） |

### 4.7 样式落点

`src/markdown/resources/markdown_default.css` 补两条（demo 在 `mainwindow.cpp`
的内联样式里，基座必须落 QSS 文件）：

```css
.mermaid-wrap { text-align: center; margin: 12px 0; }
img.mermaid { max-width: 100%; height: auto; }
```

## 五、验证方案

| 层 | 手段 |
|---|---|
| 纯函数 | `test_mermaid_svg_cleanup`：tspan 链位置累积（含 em、`xml:space`、摊平/保留外壳两种行）、marker 变换（viewBox 缩放 / markerUnits / auto-start-reverse）、空 rect 与 dasharray |
| 端到端 | `test_mermaid_renderer`：offscreen，断言 flowchart / sequence 出图、`<img ... data:image/png;base64,>`、缓存命中、坏语法返回空串。**要 `set_tests_properties(... TIMEOUT 120)`**——`cmake/test_helpers.cmake` 默认 10 秒，首次 eval 3.5MB 是秒级，10 秒会被 ctest 直接杀掉（`add_unit_test` 没暴露 TIMEOUT 参数，在 `tests/markdown/CMakeLists.txt` 里单独设） |
| 构建 | `scripts/build_ninja.bat -t relwithdebinfo -a x86`（本轮唯一口径） |
| 像素 | 量化判据（标签偏移、箭头暗像素）+ 人眼对图；素材用 `temp/` 下的 md（不入库） |
| 字体同源 | §4.4 的两条判据，作为实施第一步的"绿灯" |

## 六、文档与裁剪

### 6.1 已完成

- 许可表登记：`docs/guide/第三方依赖补丁.md`（mermaid MIT、mermaidx 的
  `dom_shim.js`/`path_bbox.js` MIT）。
- 生成物配方：`resources/extract_mermaid_payload.py`（含 md5、`--check` 自检）。
- `.gitattributes`：三份载荷标 `-text`（`autocrlf` 会让两平台 checkout 出不同
  字节，哈希对不上；`path_bbox.js` 的 LF/CRLF 之争就是这么来的）。

### 6.2 待补（实施时一并）

- `docs/guide/模板使用指南.md`：目录结构、可保留示例、按需裁剪（不要图表渲染
  的宿主怎么关掉——即 `MermaidSink` 不传即不渲染）。
- `docs/guide/发布指南.md`：体积表（现在只写了 exe 1.5MB）。

### 6.3 构建代价（要如实记）

`rcc` 把载荷编成 C 字面量数组：现有 `qrc_markdown_res.cpp` 已是 **8.5MB**
（1.6MB 载荷），加 mermaid 的三份后该生成文件会到 **~26MB**，编译时间与
`.rdata` 体积都会涨。可选优化：给 `<qresource>` 加 `compress` 属性（zlib），
加载时解压——先量一次编译时间与 exe 体积再决定。

> 2026-10-19 后记：上述体积估算已过时。三份 mermaid 载荷（以及 MathJax /
> highlight）已改为构建期 qjsc 预编译字节码，不再编进 qrc；`compress` 实测
> 与 rcc 默认压缩逐字节相同，无增益。数据与实施见
> `docs/eval/Markdown渲染qjsc预编译评估.md`。

## 七、风险与开放问题

### 7.1 已知偏差（沿用 demo 的结论）

- CSS 层叠：内联 `style=""` 本该压过非 `!important` 的作者规则，现在是被覆盖；
  同属性按文档顺序而非 specificity 决胜。mermaid 规则基本同权重，暂时安全。
- `d` 属性方向：拿"最后两个坐标对"当方向，小写相对命令与 `A` 弧会算飞；
  flowchart classic 边是绝对 M/L/C，恰好没踩到。
- journey / timeline 还需 `textPlacement:'tspan'`，本轮不覆盖这两类图。

### 7.2 线程化（下一笔）的硬约束

留档，别到时候重新踩：

- `JS_NewRuntime` / `JS_UpdateStackTop` 必须在**渲染线程**执行，栈检查基址按
  创建线程记录；UI 线程建、渲染线程用 = 配额形同虚设。
- **JS 配额 ≤ worker 线程栈**：核心里 JS 配额是软上限、线程栈是硬的。Linux 的
  QThread 默认 8MB 栈，要显式 `QThread::setStackSize`（只在 Linux 生效）。
- 若走"UI 先出占位、结果回来再 `setHtml`"：需要 generation 号丢弃过期结果，
  且第二次 `setHtml` 要恢复滚动位置（`markdown_view.cpp` 现在在
  `ApplyDocument()` 前后存/恢复）。
- 服务的宿主与退出路径要写死（`MarkdownWindow` 是懒建复用的；关窗时未完成的
  渲染怎么办、`quit()+wait()` 会不会等 5~20 秒的 eval）。
- 届时要先答一个问题：`MarkdownToHtml` 整体在渲染线程跑（sink 保持同步），
  还是 sink 异步化（接口扩成带批次）——**二选一，别混**。

## 八、实施清单（逐文件）

1. `src/markdown/resources/`：三份载荷 + 配方（**已完成**）。
2. `markdown_res.qrc`：三条 `<file>`（**已完成**）。
3. `.gitattributes`、`docs/guide/第三方依赖补丁.md`（**已完成**）。
4. `mermaid_svg_cleanup.{h,cpp}`：文本定标（含 `xml:space`、摊平/保留外壳）、
   marker 展开、CSS 解析/选择器匹配/属性写回 + 字体族强制（§4.4-3）、
   空 rect / dasharray。
5. `mermaid_renderer.{h,cpp}`：运行时、回调、pump、SVG→PNG、缓存、dump 开关。
6. `markdown_to_html.{h,cpp}`：`MermaidSink` + 围栏分支。
8. `markdown_view.cpp`：`ApplyDocument` 里传 `MermaidToImgTag`。
9. `markdown_default.css`：§4.7 两条。
10. `tests/markdown/`：两条用例 + CMake 注册（含 TIMEOUT 120）。
11. `src/markdown/CMakeLists.txt`：新增源文件；载荷体积与 `compress` 复量。
12. 文档：`模板使用指南.md`、`发布指南.md`（§6.2）。

提交拆四笔：资产与许可 / 后处理两层 / 渲染器 / 管线接入与样式。

## 九、审查记录

一轮审查（subagent，只读），18 条；处置如下。

### 🔴 阻塞（5 条，全部已改）

| # | 问题 | 处置 |
|---|---|---|
| 1 | D3 线程模型与同步 sink 自相矛盾（request id 塞不进 `std::function`；formula 侧"只许渲染线程访问"与现有 UI 线程同步调用冲突） | **采纳**：本轮砍掉异步，接口保持同步（D5）；`MarkdownToHtml` 在哪个线程调用写死为"调用方线程"，线程化时二选一（§7.2） |
| 2 | "两载荷共用一个 runtime"与"线程内自持"矛盾，且无实施步骤；`dom_shim` 会改 `globalThis` 上的 console/CSS/URL/crypto 等 | **采纳**：改为两个载荷各持一个 runtime（D3），并补上证据与理由 |
| 3 | D2 的"量与画同源"无判据，且依赖"Qt 会把 font-family 从祖先继承到 `<text>`"这条从未验证的行为 | **采纳**：改成"每个元素都强制写 font-family"（不依赖继承），度量用 `QFontMetricsF` 而非 `QRawFont`（后者不做字体替换），并给出两条判别实验（§4.4） |
| 4 | 预热放构造函数：既占 UI 时间（窗口懒建、构造在 UI 线程、eval 5~20 秒），又踩 `JS_NewRuntime`/`JS_UpdateStackTop` 的线程亲和 | **采纳**：本轮不做预热（同步版没有后台可占）；线程亲和与预热时机写进 §7.2 |
| 5 | JS 配额 64MB > 宿主 32MB 线程栈，且没说 worker 线程栈；Linux QThread 默认 8MB | **采纳**：配额改 16MB（与 formula 一致），"JS 配额 ≤ 线程栈"写成硬约束（§4.6、§7.2） |

### 🟡 建议（9 条，8 条已改）

| # | 问题 | 处置 |
|---|---|---|
| 6 | 端到端用例会被 ctest 的 10 秒超时杀掉 | 采纳：用例单独设 `TIMEOUT 120`（§5） |
| 7 | 用例命名不符约定（`test_*`） | 采纳：`test_mermaid_svg_cleanup` / `test_mermaid_renderer` |
| 8 | "移植"盖住了强制的风格改写（pragma once、camelCase、4 空格、`SkipEmptyParts`） | 采纳：单列 §4.6，含"Qt 5.12 别升级成 `Qt::SkipEmptyParts`" |
| 9 | demo 的调试转储必须显式处理 | 采纳：默认关闭 + 环境变量开关、不落 exe 旁（§4.5） |
| 10 | "逐字节一致"这条验收不成立（`path_bbox.js` 4823 vs 4690，CRLF 之差） | 采纳：以 LF 字面量为准并改配方哈希，教程里那句改成"按哈希对、别数字节"，加 `.gitattributes -text` |
| 11 | `.mermaid-wrap` / `img.mermaid` 在基座没有落点 | 采纳：落到 `markdown_default.css`（§4.7） |
| 12 | 异步回填会破坏滚动位置与旧结果覆盖新文档 | 采纳：写进 §7.2 的线程化约束（本轮无异步，不触发） |
| 13 | 线程/服务的宿主与退出路径没定 | 采纳：并入 §7.2 待答项 |
| 14 | 文档面比设计写的多（模板使用指南、发布指南体积表、qrc 编译体积） | 采纳：§6.2、§6.3；体积如实记录 |

### 🔵 可选（4 条）

| # | 问题 | 处置 |
|---|---|---|
| 15 | 接口章节与 D3 服务冲突 | 采纳：D5 已统一为同步接口，服务一节删掉 |
| 16 | `mermaid.initialize` 配置没进设计 | 采纳：§4.3"不得丢"清单 |
| 17 | 围栏分支的实现约定没写清（sink 拿反转义后的源码） | 采纳：写进 §4.1 |
| 18 | 体例与既有设计文档不一致 | 采纳：本文按「问题陈述 / 现状回顾 / 决策记录 / … / 实施清单 / 审查记录」重排 |

**未采纳**：无。唯一"部分采纳"是 #4：预热本身推迟到线程化那一笔，但线程亲和
与 `stack_top` 的坑先留档。
