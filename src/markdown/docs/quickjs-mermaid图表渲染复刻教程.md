# QuickJS + Mermaid 图表渲染：从零复刻教程

> **定位（先读这段）**：本文是试验田（qt-md-renderer-demo）时期的"从零复刻"
> 实录，操作步骤按那边的仓库布局写的。**在本仓照抄复刻不出来，也不需要**——
> 这套能力已由 `src/markdown` 交付并带测试。本文的价值是踩坑记录与方案推理；
> 要在全新项目里重走这条路，demo 特有的操作按下表替换：
>
> | 文中（demo） | 本仓对应 |
> |---|---|
> | 三份资产手工拷 `dist/mermaid/` | `src/markdown/resources/` 同名三份，`extract_mermaid_payload.py --check` 对哈希 |
> | 字体：`dist/mermaid/fonts/CJK.ttf`（SimHei）+ 自解析 TTF 度量（Step 2 全章） | **路线被否**：SimHei 不可再分发；本仓是系统字体 + `QFontMetricsF` 量画同源（`mermaid_renderer.cpp` 的 `PickCjkFamily`），Step 2 别照抄 |
> | QuickJS 解压 + `sb.ps1` 手工补丁 | `patches/quickjs/` + `cmake/patch_dependencies.cmake`（configure 期自动应用） |
> | 分阶段转储 `dbg_N_*.svg` | `MD_MERMAID_DUMP` 环境变量（同名机制，随 `MarkdownView` 生效） |
> | GUI 接入与三层后处理 `svg_cleanup` 等 | `src/markdown/mermaid_renderer.cpp` + `mermaid_svg_cleanup.{h,cpp}`；设计决策见 `docs/plan/mermaid图表渲染设计.md` |
> | 量化验收 `temp/measure_render.py`（funq 抓图） | `tests/markdown/mermaid_renderer_test`（端到端无头）；像素探针尚未落 `scripts/funq/`（todo 有账） |
>
> **仍然成立的部分**：三份资产的来源与"逐字节照搬"口径、eval 顺序
> （bridge→path_bbox→dom_shim→mermaid）、`htmlLabels:false`、Promise 泵、三层
> 后处理顺序（cleanSvg→inlineSvgCss→inlineSvgMarkers）、踩坑总表（M1–M18）。
> 载荷重建看同目录《JS载荷复刻指南》。

## 这条路证明了什么

**结论：走通了。** 不引 Node.js、不引 Qt WebEngine、不起外部进程：**QuickJS 里跑
官方 `mermaid.js` 出 SVG，Qt 把 SVG 画成 PNG，以 data URI 内联进 `<img>`**，
交给 `QLiteHtmlWidget` 显示。实测覆盖 flowchart / sequence / class / state /
pie 五类图，含多行标签、自消息、Note、继承空心三角，Windows 与 WSL 两侧表现一致。

它和 `quickjs-mathjax公式渲染复刻教程.md` 那条路**不是一回事**，别照搬：

| | MathJax | Mermaid |
|---|---|---|
| 排版 | 纯计算，`liteAdaptor` 自带无 DOM 适配器 | 布局引擎（dagre）**必须知道文字宽高**才决定连线怎么走 |
| 度量 | 不需要 | 靠宿主回答 `getBBox()` / `getComputedTextLength()` |
| 产物 | 全是 `<path>`，QSvgRenderer 直接画 | 带 CSS 类的 SVG：`<text>`/`<marker>`/`<style>` 块，样式 99% 在类规则里 |

所以多了两件事：**给 mermaid 造一个假 DOM（借 mermaidx 的 `dom_shim.js`）+ 自己
回答文字度量（自解析 TTF）**，以及**一层把"浏览器式 SVG"翻译成 QSvgRenderer
能画的东西的兼容层**——后者是这个移植 70% 的工作量所在。

mermaidx（Python 包，MIT）在这里的作用是**资产与思路来源**，不是运行时依赖：
`dom_shim.js` / `mermaid.js` / `PATH_BBOX_JS` 三样逐字节照搬（实测 `diff` 无差异），
字体度量按它 `font_metrics.py` 的口径用 C++ 重写。

## 起点

工作区 `D:\trae_workspace\qt-md-renderer-demo`，起点是 `d1fa731`（LaTeX 那条管线
已落地）。复刻时卡住就看这些文件，它们是"标准答案"：

| 文件 | 内容 |
|---|---|
| `src/mermaid_renderer.{h,cpp}` | QuickJS 集成、三个 FFI 回调、render + Promise pump、SVG→PNG |
| `src/ttf_metrics.{h,cpp}` | TTF 四表解析、按 weight 选字体、度量 |
| `src/svg_cleanup.{h,cpp}` | 文本定标烘焙、marker 展开、空 rect / dasharray 修补 |
| `src/svg_css_inliner.{h,cpp}` | CSS 规则内联成元素属性 |
| `dist/mermaid/` | 载荷（未入库，见 Step 1） |
| `temp/measure_render.py` | 把渲染结果量成数字（见「怎么看见渲染结果」） |

---

## Step 0：配置构建目录

```bat
call "D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /D D:\trae_workspace\qt-md-renderer-demo
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug
ninja -C build
```

之后每次编译用 `temp\build_demo.bat`（vcvars + ninja 两行），或直接
`ninja -C build`。

## Step 1：取资产（mermaidx-0.9.5）

参考包在 `D:\software\aa-sources\mermaidx-0.9.5`（压缩包 `.tar.gz` 同目录）。

```powershell
New-Item -ItemType Directory -Force -Path "dist\mermaid\fonts" | Out-Null
Copy-Item "...\mermaidx-0.9.5\mermaidx\assets\dom_shim.js"  "dist\mermaid\"
Copy-Item "...\mermaidx-0.9.5\mermaidx\assets\mermaid.js"   "dist\mermaid\"
Copy-Item "...\mermaidx-0.9.5\mermaidx\assets\fonts\*.ttf"  "dist\mermaid\fonts\"
```

第四样 `path_bbox.js` 是 `mermaidx/path_bbox.py` 里一段 `PATH_BBOX_JS` 字符串常量，
**别 import 整个包**（会拉可选依赖，见 M1），用正则抠出来：

```python
import re, pathlib
src = pathlib.Path(r"...\mermaidx-0.9.5\mermaidx\path_bbox.py").read_text(encoding="utf-8")
m = re.search(r'PATH_BBOX_JS\s*=\s*r?"""(.*?)"""', src, re.S)
pathlib.Path(r"dist\mermaid\path_bbox.js").write_text(m.group(1), encoding="utf-8")
```

> **验收标准：按内容哈希对，别数字节。** 三份的 md5：
> `mermaid.js` `1dd12fadbfa258c3ac600e9e7f1d6a3d`、`dom_shim.js`
> `d8d380c55772e2868cdd075d4c42a5c5`、`path_bbox.js`
> `8f82e27f69973c1e31fd488dfa67435e`。
>
> `path_bbox.js` 的字节数是个陷阱：它抠出来是 **LF** 的 4690 字节，而
> PowerShell 的 `write_text()` 会把 `\n` 翻成 `\r\n`，落盘就成了 4823 字节。
> 字节数对不上先比哈希，别急着怀疑上游。三份都是纯 JS，跨平台共用，Node 只在
> 升级 mermaidx 时需要；入 git 时记得标 `-text`（见基座仓库的 `.gitattributes`），
> 否则 `autocrlf` 会让两个平台 checkout 出不同字节，哈希对不上。

中文字体是第四件事，`dist/mermaid/fonts/CJK.ttf`（见 M2 的许可警告）。

## Step 2：TTF 字体度量

mermaid 布局时问宿主：`getBBox()` → `__measureTextFull`、`getComputedTextLength`
→ `__measureText`。这两个数必须和"最终画字用的字体"一致，否则文字溢出或偏。

只读四张表，**不解析字形轮廓**：

| 表 | 用途 | 关键字段 |
|---|---|---|
| `head` | 字体总单位数 | `unitsPerEm`（offset 18，u16） |
| `hhea` | 上升/下降/计量数 | `ascender`(4, i16)、`descender`(6, i16)、`numberOfHMetrics`(34, u16) |
| `hmtx` | 每字形 advance | `advanceWidth[i] = u16(hmtx + i*4)` |
| `cmap` | 码点 → 字形 id | 优先 (3,10)>(3,1)>(0,4)>(0,3)，支持 fmt 4/12 |

```text
scale  = size_px / unitsPerEm
width  = Σ hmtx[cmap[cp]] × scale
ascent = ascender × scale
descent = -descender × scale      // descender 在字体里是负数，取反成正数
```

按 weight 选字体（与 mermaidx 的 `get_font` 同规则）：`weight >= 600` 或字符串
`bold`/`bolder` 用 Bold 文件，其余 Regular。

> **先做对照验证再往下走。** 用 Python 加载同一份字体算几个基准值
> （DejaVu Sans 16px：`upem=2048 asc=1901 desc=-483`，`"Hello, world"` →
> `94.78125`），C++ 侧要跑出完全相同的小数点。后面所有"文字偏移"问题都从
> 这里追起，这里不准，后面全是玄学。

## Step 3：QuickJS 集成

### 3.1 三个 FFI 回调

| JS 全局名 | C++ 侧 | 返回 |
|---|---|---|
| `__log_raw(s)` | `JS_NewCFunction` | `JS_UNDEFINED` |
| `__measureText_raw(t,s,f,w,st)` | 5 参数 | `JS_NewFloat64` |
| `__measureTextFull_raw(...)` | 5 参数 | `JS_NewStringLen`（**JSON 字符串**） |

`__measureTextFull_raw` 返回的是字符串不是对象，JS 侧包一层 `JSON.parse`：

```js
globalThis.__log = (s) => __log_raw(s);
globalThis.__measureText = (t,s,f,w,st) => __measureText_raw(t,s,f,w,st);
globalThis.__measureTextFull = (t,s,f,w,st) =>
    JSON.parse(__measureTextFull_raw(t,s,f,w,st));
```

### 3.2 eval 顺序：bridge → path_bbox → dom_shim → mermaid

```cpp
JS_SetMaxStackSize(rt, 64u << 20);   // 与 /STACK 配套，见 Step 7
// 1) 注册 3 个 raw 回调 → 2) eval 上面的 bridge
// 3) 依次 eval（首次要几秒，3.5MB 的 bundle）
evalGlobal(path_bbox_js, "path_bbox.js");
evalGlobal(dom_shim_js,  "dom_shim.js");
evalGlobal(mermaid_js,   "mermaid.js");
// 4) mermaid.js 是 esbuild 打的 ESM，手动暴露 default
//    globalThis.mermaid = globalThis.__esbuild_esm_mermaid_nm.mermaid.default
```

### 3.3 render + Promise pump

`mermaid.render()` 返回 Promise，而 **QuickJS 的微任务不会自动跑**，必须手动泵：

```cpp
evalGlobal("globalThis.__renderResult=null; globalThis.__renderError=null;"
           "mermaid.render(__renderId, __code)"
           "  .then(r => { globalThis.__renderResult = r.svg; })"
           "  .catch(e => { globalThis.__renderError = String(e); });");
for (int i = 0; i < 200000; ++i) {            // 上界与 mermaidx 同值
    const int r = JS_ExecutePendingJob(rt, &pctx);
    if (r < 0) break;                          // 异常，去读 __renderError
    if (r == 0) break;                         // 没 job 了
    if (结果槽非 null) break;
}
```

> `dom_shim.js` 把 `requestAnimationFrame` / `setTimeout` 都替成了微任务，
> 所以这个循环能收敛；mindmap 这类图会不停排 rAF，靠 job 上界兜底。

### 3.4 `initialize` 的关键配置

```js
mermaid.initialize({
  startOnLoad: false, theme: 'default',
  htmlLabels: false,                     // ← 关键，见 M3
  flowchart: { htmlLabels: false },
  class: { htmlLabels: false },
  state: { htmlLabels: false },
  er: { useMaxWidth: true }
  // journey / timeline 还需要 textPlacement:'tspan'，否则走 foreignObject
});
```

## Step 4：分阶段转储——先让问题可见

**这一步不是可选项。** QuickJS 出的 SVG 到 QSvgRenderer 之间隔着三层修补，
出了问题是"哪一层"的问题靠猜没用。在 `svgToMermaidImgTag` 里按阶段落盘：

```text
dbg_N_raw.svg      mermaid.render() 的原样输出
dbg_N_clean.svg    删空 rect / 文本定标之后
dbg_N_inlined.svg  CSS 内联之后
dbg_N_marked.svg   marker 展开之后（= 真正交给 QSvgRenderer 的）
dbg_N.png          最终光栅化结果
```

配合 `temp/measure_render.py` 把结果量成数字：标签相对节点框心的偏移、
消息线末端的箭头像素数（0 = 箭头没画出来）、以及直接看 PNG。

> **`QFile` 不 `open()` 直接 `write()` 是静默失败**——第一版 `dbg_bare.svg`
> 就从来没生成过，白白怀疑了半天转储位置（M4）。

## Step 5：SVG 后处理三层（顺序不可颠倒）

```text
raw SVG
  ↓ ① cleanSvg          删空 rect、dasharray 修补、烘焙 tspan 位置
  ↓ ② inlineSvgCss      CSS 规则内联成元素属性
  ↓ ③ inlineSvgMarkers  marker 展开成 <g>
final SVG → QSvgRenderer
```

顺序为什么不能颠倒：

- **烘焙文本位置必须在 CSS 内联前**：CSS 里有 `.flowchart-label text { text-anchor:
  middle }` 这类规则，先内联才能作用到烘焙后新建的 `<text>` 上。
- **marker 展开必须在 CSS 内联后**：marker 里 `<path>` 的颜色来自类规则。
  sequence 的 `<line class="messageLine0" stroke="none">` —— **属性上写的就是
  `none`**，真颜色在 CSS 里；早跑一步读到的就是 `none`，箭头画成隐形的（M5）。

### 5.1 cleanSvg

三件事：

**① 删无尺寸的 `rect.background`**：htmlLabels:false 下没人给它填尺寸，
QSvgRenderer 会画成黑方块。

**② `stroke-dasharray="0"` → `"none"`**：mermaid 用 `0` 表示实线，
QSvgRenderer 理解成"虚线长 0"→ 连线整条不画。

**③ 烘焙 tspan 位置（本次最难的一处）**

mermaid 把真正的基线写在定位 tspan 上：

```xml
<text y="-10.1">
  <tspan class="text-outer-tspan row" x="0" y="-0.1em" dy="1.1em">…</tspan>
</text>
```

按 SVG 规范基线 = `text.y` 被 `tspan.y` 覆盖，再加 `dy`；但 **Qt 的 `QSvgTspan`
内部根本不存 x/y/dx/dy**（`qsvggraphics_p.h` 里只有 `m_text`/`m_mode`/`m_isTspan`），
一个 `<text>` 下所有 tspan 都被当成同一行画在 `<text>` 自己的坐标上 → 文字整体
**偏高约 24 单位**（实测：标签中心落在节点框上边缘上）。

解法：把 tspan 链上的位置按 SVG 规则累积出来、烘焙回 `<text>`：

```cpp
void AccumulateAttrs(const QString& tag, double font_size, TextPos* pos) {
    if (ParseLen(AttrOf(tag, "x"), font_size, &v)) pos->x = v;      // 绝对覆盖
    if (ParseLen(AttrOf(tag, "y"), font_size, &v)) pos->y = v;
    if (ParseLen(AttrOf(tag, "dx"), font_size, &v)) pos->x += v;    // 相对累加
    if (ParseLen(AttrOf(tag, "dy"), font_size, &v)) pos->y += v;
}
```

`ParseLen` 必须认 `em`（`-0.1em` → `-1.6`）：**QSvgRenderer 5.12 不解析 em 单位**，
当 0 处理。

配套三条，缺一不可：

- **多行拆成多个 `<text>`**：Qt 反正把所有 tspan 当一行画，拆开才有多行。
- **不能 tspan 套 tspan**：Qt 只接受 tspan 作为 `<text>` 的**直接**孩子，
  嵌套时报 `qt.svg: Could not add child element to parent element because the
  types are incorrect`（Linux 侧才看得见，见 M6）。所以行里已经有 run tspan
  就**摊平**（run 直接当 `<text>` 的孩子），行是叶子才包一层。
- **外壳不能丢**：CSS 常按 `text.actor>tspan` 定位，少一层规则就匹配不上，
  actor 名字会退回盒子的填充色（淡紫）。行属性被剥光也要留个光杆 `<tspan>`。

还要给烘焙出的 `<text>` 加 **`xml:space="preserve"`**：mermaid 把 run 之间的
空格写在后一个 run 的开头（`qP` 里 `" " + content`），**Qt 默认空白模式会吃掉
它**——class 图 `+String name` 会变成 `+Stringname`（M7）。

### 5.2 inlineSvgCss：QSvgRenderer 不认 `<style>`

mermaid 的样式 99% 在 `<style>` 块里，QSvgRenderer 完全不看。要写一个简易
CSS 解析器 + 选择器匹配器：

- **选择器**：`#id`、`.class`、`tag`、`tag.class`，后代组合与子选择器 `>`。
  维护祖先栈、**从右往左**匹配整条链（只匹配最后一段会漏掉 `#m0 .node rect`
  这类规则）。
- **优先级**：QSvgRenderer 5.12 处理 style vs attribute 的顺序跟 CSS 规范相反
  ——它认 attribute。所以命中 CSS 的 `fill`/`stroke`/`font-size` 等要
  **同时改 presentation attribute**。
- **实体**：`<style>` 里的 `font-family:&quot;trebuchet ms&quot;`，`&quot;` 是
  6 个字面字符。直接按 CSS 语法 `split(';')` 会把 `;` 当声明分隔符，值变成
  `&quot`，属性损坏、整份 SVG 解析失败。流程必须是
  **抽出 CSS → 解码实体 → 按 CSS 解析 → 写回时再编码**。

> 已知偏差（没咬到，但记着）：真正的 CSS 层叠里，元素自带的 `style=""` 优先于
> 任何非 `!important` 的作者规则；现在实现是"先拿内联播种、再让规则覆盖"，
> 且同属性按文档顺序而非 specificity 决胜。mermaid 的规则基本同权重，暂时
> 安全；哪天要做 font-family 强制替换就会撞上（M8）。

### 5.3 inlineSvgMarkers：QSvgRenderer 不画 marker

mermaid 的箭头全是 `<marker>`；QSvgRenderer 不画，还会把 marker 里的 path
**误画到原点**（左上角黑块），而删掉定义又会让引用悬空、整条 path 不画。

解法：在端点处就地展开成 `<g>`：

```text
<g transform="translate(端点) rotate(方向) scale(viewBox→viewport) translate(-refX,-refY)">
  <!-- marker 内容照抄 -->
</g>
```

```cpp
// 有 viewBox：缩放 = markerWidth / viewBox.width；refX/refY 也在 viewBox 坐标里
// markerUnits 缺省是 strokeWidth：还要再乘线宽（mermaid 大多显式写 userSpaceOnUse）
// orient="auto-start-reverse" 时**起点**箭头才翻 180°
double angle = std::atan2(dir.y(), dir.x()) * 180.0 / M_PI;
```

五条硬规矩：

- **必须是原图形的兄弟节点**，插在 `</line>` / `</path>` **之后**。塞进
  `<line>` 里 SVG 根本不画（箭头消失的第二次）——而且 mermaid 的 `<line>` 是
  有闭合标签的，要主动跳过它。
- **起点方向用"路径前进方向"**（`pts[1] - pts[0]`），不是指回起点；只有
  `orient="auto-start-reverse"` 才翻 180°。取反了 class 图的继承空心三角会
  画到路径另一侧。
- **`marker-start` / `marker-end` 都要处理**，`marker-mid` 忽略；处理完把
  `marker-*` 属性删干净，最后删掉所有 `<marker>` 定义。
- **三种图形**：`<path>`、`<line>`（sequence 的消息线就是它）、`<polyline>`。
  只匹配 `<path>` 时 sequence 的箭头一个都不出。
- 箭头几何照 marker 的真实定义算，别硬编码三角形（不同图类型的箭头形状、
  refX、markerWidth 都不同）。

## Step 6：接进 markdown 管线

`markdown_to_html.cpp` 的 `postProcessCodeBlocks` 里加一个分支：

```cpp
if (lang == QLatin1String("mermaid")) {
    const QString img = mermaidToImgTag(codeHtml);
    if (!img.isEmpty()) {
        result += "<div class=\"mermaid-wrap\">" + img + "</div>";
    } else {
        result += buildCodeBlock(highlightCode(codeHtml, lang));  // 失败退回代码块
    }
    pos = codeEnd + kEndTag.size();
    continue;
}
```

配套：

- **缓存**：`mermaidToImgTag` 用 `QHash<QString, QString>` 按源码缓存
  （失败也缓存空串，别每次编辑重试坏图）。
- **样式**：`.mermaid-wrap { text-align: center; margin: 12px 0; }`、
  `img.mermaid { max-width: 100%; height: auto; }`。
- **尺寸从 viewBox 读**（mermaid 根元素是 `width="100%"`），2x 超采样后
  `<img>` 按 1 倍摆。

## Step 7：CMake

```cmake
find_package(Qt5 5.12 QUIET COMPONENTS Svg)   # QSvgRenderer
target_link_libraries(${TARGET_NAME} PRIVATE Qt5::Svg qjs)
if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /utf-8)   # 中文注释按 GBK 读会炸
    # mermaid 在 quickjs 里解析递归深：JS_SetMaxStackSize 的配套线程栈。
    # 32 位目标别贪大，主线程栈 reserve 会吃掉地址空间
    target_link_options(${TARGET_NAME} PRIVATE "/STACK:33554432")
endif()
```

---

## Linux / WSL 验证（实测通过）

环境：WSL2 Ubuntu 20.04，**系统 Qt 5.12.8**、GCC 9.4、cmake 3.31.12
（`~/env_software` 那份）、ninja。exe 在 `build/bin/`，到仓库根正好两级
`../../`，`dist/mermaid/` 的路径在两边都成立。

```bash
rsync -a --exclude build/ --exclude .git/ --exclude node_modules/ --exclude local.cmake \
    /mnt/d/trae_workspace/qt-md-renderer-demo/ /home/zhou/works/qt-md-renderer-demo/
cd /home/zhou/works/qt-md-renderer-demo && ninja -C build
QT_QPA_PLATFORM=offscreen ./build/bin/qt-md-renderer-demo temp/test_mermaid.md
```

`QT_QPA_PLATFORM=offscreen` 就够了——整条链路（QuickJS → SVG → QSvgRenderer →
PNG）都不需要窗口，转储照常落盘，不用折腾 WSLg。

实测结果：

| 指标 | Windows | WSL |
|---|---|---|
| flowchart 标签相对框心 | +1.75 / +2.0 | +1.1 ~ +2.1 |
| sequence 消息线末端箭头像素 | 230 / 68 | 230 |
| `qt.svg` 警告 | 不可见（见 M6） | **10 → 0** |

残留差异：Linux 上 state 图标签偏 +3.0~+3.5（比 Windows 大 1 左右），根因是
**度量用 SimHei、渲染用系统 fallback 字体**——就是 M2 那条"量/画不同源"。

> **rsync 的两个坑**：`local.cmake` 是**每台机器一份**的（Windows 指
> `D:/Qt/...`、Linux 留空），必须 `--exclude`，否则一边的配置被另一边的覆盖；
> 从 Windows 侧调 rsync 时 `/mnt/d/...` 会被 MSYS 转成 `D:/...`（rsync 当成
> 远端主机），整条命令交给 `wsl.exe bash -lc "..."` 跑才安全。

## 怎么"看见"渲染结果

三个手段，按信息量排序：

1. **分阶段转储**（Step 4）——定位"哪一层改坏了"。
2. **把渲染量成数字**（`temp/measure_render.py`）：从 raw SVG 里读节点框几何、
   从 PNG 里读墨迹行，算出"标签中心相对框心的偏移"；在消息线末端开个窗口数
   暗像素，**0 就代表箭头没画出来**。比肉眼可靠，也能跨平台对比。
3. **直接看 PNG**：转储出来的 `dbg_N.png` 就是最终像素，PNG 带 alpha，
   量之前先合成到白底（透明像素按 0 算会把统计全带偏）。

## 踩坑总表

`M*` 按踩到的顺序编号。

### 资产 / 环境

| # | 症状 | 根因 | 修法 |
|---|---|---|---|
| M1 | `ModuleNotFoundError: No module named 'termaid'` | `import mermaidx` 会拉可选依赖 | 别 import 整个包：纯文本正则抽 `PATH_BBOX_JS` |
| M2 | 中文全变方块 / 发布合规 | DejaVu Sans 不含 CJK；**SimHei 授权只到 Windows 系统内，不可再分发** | 换 Noto Sans SC / 思源黑体（OFL），或只做子集；`resolveFontPaths()` 里换个路径即可 |
| M3 | flowchart 里文字全丢 | 默认 `htmlLabels:true` 把文字放进 `<foreignObject>`，QSvgRenderer 完全不认 | `htmlLabels:false`（flowchart/class/state 各自也要写） |
| M4 | 转储文件从没生成过 | `QFile` 不 `open()` 直接 `write()` 静默失败 | 先 `open(QIODevice::WriteOnly)` 再写 |
| M5 | sequence 箭头不画 | 箭头生成了但读的是 `<line stroke="none">`；真颜色在类规则里，而 marker 展开跑在 CSS 内联**之前** | marker 展开排到 CSS 内联之后 |
| M6 | Linux 侧刷 `Could not add child element ... types are incorrect` | tspan 套 tspan；**Windows GUI 子系统程序的 stderr 不进控制台，qDebug 走 OutputDebugString，重定向抓不到**——所以这个坑在 Windows 上"看不见" | 摊平内层 tspan；跨平台验证一定要在 Linux 侧看日志 |
| M7 | class 图 `+String name` → `+Stringname` | Qt 默认空白模式吃掉 run 的前导空格 | 烘焙出的 `<text>` 加 `xml:space="preserve"` |
| M8 | CSS 层叠顺序与规范相反 | 内联 `style=""` 本该压过非 `!important` 的作者规则，现在是被覆盖；同属性也没按 specificity 排序 | 记着这处偏差；真要修就按 (id, class, tag) 排序并区分内联优先级 |

### QuickJS / C++

| # | 症状 | 根因 | 修法 |
|---|---|---|---|
| M9 | `mermaid.render()` 永远不 resolve | QuickJS 微任务不自动执行 | `JS_ExecutePendingJob` 循环泵，带上界 |
| M10 | `globalThis.mermaid` 是 undefined | mermaid.js 是 esbuild 的 ESM 产物 | 手动 `= __esbuild_esm_mermaid_nm.mermaid.default` |
| M11 | `__measureTextFull` 拿到字符串 | 契约是 JSON 字符串 | JS 侧包 `JSON.parse` |
| M12 | 编译报"`=` 前面没有变量名" | 局部变量/lambda 取名 `emit`——**那是 Qt 的宏**，展开成空 | 换个名字（`build_text`） |
| M13 | GCC 报 `M_PI` 未定义 | MSVC 的 `<cmath>` 不给 `M_PI` | 自己 `constexpr double kPi` |
| M14 | SVG 结构非法、`renderer.isValid()` 假、整张图不画 | 拼标签时把**完整标签**当属性串拼了（`<tspan` 存进了 `open_tag`），拼出 `<tspan<tspan …>` | 存属性就只存属性（`TagAttrs()` 剥掉尖括号） |
| M15 | 首次 render 卡 5~20 秒 | 3.5MB bundle 的 eval + 布局，是单次成本 | 懒加载 + 结果缓存；要更顺滑见「后续可改进」 |

### 渲染上下文

| # | 症状 | 根因 | 修法 |
|---|---|---|---|
| M16 | 拿不到图尺寸 | mermaid 根元素是 `width="100%"` | 从 `viewBox` 读宽高 |
| M17 | 曲线边缘发毛 | 直接按原尺寸画 | 2x 超采样：`QImage(w*2, h*2)` + `<img>` 按 1 倍摆 |
| M18 | 图左上角有黑块 | QSvgRenderer 把 `<marker>` 里的 path 当普通节点画到原点 | 删所有 `<marker>` 定义（内容已经展开成 `<g>`） |

## 验证清单

```bash
# Windows
build\bin\run_qt-md-renderer-demo.bat temp\test_mermaid.md
# WSL
QT_QPA_PLATFORM=offscreen ./build/bin/qt-md-renderer-demo temp/test_mermaid.md
```

- [ ] flowchart TD/LR：节点文字居中（相对框心偏移应在 ±3 内）、箭头指向正确
- [ ] 多行标签（`<br/>`）：每行独立成行、行距正常
- [ ] sequence：实线/虚线消息都有箭头、自消息成环、Note 框、多行标签
- [ ] class：继承**空心三角**贴着父类框、关系标签在位
- [ ] state / pie：文字位置正确
- [ ] 坏语法（比如 `flowchart TD A[` ）：退回代码块显示，不白屏
- [ ] Linux 侧 `qt.svg` 警告为 0

## 后续可改进的点

- **性能**：mermaid 块多时，每次编辑都同步跑 QuickJS 会卡。可把渲染挪到后台
  线程 + 增量更新；注意 `JSRuntime` 非线程安全，得加锁或每线程一个 runtime。
  LaTeX 那条路同理（每次都是同步 convert）。
- **量/画同源**：现在度量读 TTF、渲染走系统字体（M2）。彻底解法是把 mermaid
  的 `<text>` 转成 `<path>`（自己用 QPainterPath::addText 出路径），或者
  注册字体 + 把 SVG 里的 `font-family` 强制替换成注册后的族名。
- **CSS specificity**（M8）：做上面的 font-family 替换时会正撞上。
- **`d` 属性解析**：现在拿"最后两个坐标对"当方向。小写相对命令和 `A` 弧命令
  会算飞；flowchart classic 边是绝对 M/L/C 恰好没踩到，换图类型要补。
- **journey / timeline**：要补 `textPlacement:'tspan'`，否则走 foreignObject。
- **许可**：入库前把 CJK 字体换成 OFL 的（M2），`mermaid.js` 与 `dom_shim.js`
  是 MIT，记得在依赖清单里登记。
