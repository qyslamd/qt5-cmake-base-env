# QuickJS + MathJax 公式渲染：从零复刻教程

> **定位（先读这段）**：本文是试验田（qt-md-renderer-demo）时期的"从零复刻"
> 实录，操作步骤按那边的仓库布局写的。**在本仓照抄复刻不出来，也不需要**——
> 这套能力已由 `src/markdown` 交付并带测试。本文的价值是踩坑记录与方案推理；
> 要在全新项目里重走这条路，demo 特有的操作按下表替换：
>
> | 文中（demo） | 本仓对应 |
> |---|---|
> | `sb.ps1` 手工补 quickjs | `patches/quickjs/` + `cmake/patch_dependencies.cmake`（configure 期自动应用） |
> | `dist/mathjax-svg.min.js` 运行期相对路径加载 | qrc 内嵌 `src/markdown/resources/mathjax-svg.min.js` |
> | 打包入口 `tools/mathjax-entry.mjs` | `src/markdown/resources/mathjax_svg_entry.mjs`（头注释即配方） |
> | 跑车 `mathjax_smoke` / `sb` | `tests/markdown/formula_renderer_test`（端到端、无头、进 ctest） |
> | GUI 接入 `src/mainwindow` + `formula_renderer` | `src/markdown/formula_renderer.{h,cpp}` + `MarkdownView` |
> | `/STACK` 64MB + 配额 64MB | `/STACK` 32MB（INTERFACE）+ JS 配额 16MB——32 位地址空间只有 2GB，见模块 CMakeLists 注释 |
>
> 载荷重建与校验的**操作口径**看同目录《JS载荷复刻指南》；行为判据看
> `tests/markdown/`。踩坑总表（F1–F9）与方案层推理（liteAdaptor、fontCache、
> em/ex 度量、两个 `--define`）在本仓全部仍然成立。

## 这条路证明了什么

**结论：走通了。** litehtml 排不动 KaTeX 的 vlist 布局（`inline-table` 不收缩、
负偏移塌陷，实测分式分数线拉成通栏宽、上标消失），但画图没有任何问题。所以
把排版交给 MathJax：**QuickJS 里跑 `tex→svg`，Qt 把 SVG 画成 PNG，以 data URI
内联进 `<img>`**。最终 `E=mc²`、分式、积分、求和全部达到浏览器级排版，且公式
渲染与 HTML 引擎解耦——换掉 litehtml 也不影响这条管线。

## 起点

工作区回到最后一次干净提交（复刻前先清场）：

```bat
git status                     -- 确认起点 c69b6a4
git checkout -- .              -- 丢弃未提交改动
git clean -fd 3rdparty/ node_modules/ dist/   -- 清掉未跟踪的第三方与产物
```

复刻过程中卡住时，本工作区当前未提交的改动就是"标准答案"，重点参考这些文件：

| 文件 | 内容 |
|---|---|
| `tools/mathjax-entry.mjs` | 打包入口（Step 3） |
| `dist/mathjax-svg.min.js` | 打包产物（1.5MB） |
| `tests/mathjax_smoke.cpp` + `tests/CMakeLists.txt` | 独立跑车测试 `mathjax_smoke`（Step 5） |
| `src/mainwindow.{h,cpp}` | GUI 接入（Step 6） |
| `src/CMakeLists.txt` | QtSvg + /utf-8 + /STACK（Step 7） |

---

## Step 0：配置构建目录

worktree / 新克隆里没有 `build/`，先建（Ninja + Debug + x64，与本仓既有口径一致）：

```bat
call "D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /D <仓库根>
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

`local.cmake`（Qt 路径指向 `D:/Qt/Qt5.12.12/5.12.12/msvc2017_64`）已入库，
worktree 里自动就有，不必手工补。之后每次编译：

```bat
ninja -C build          :: 全部
ninja -C build mathjax_smoke   :: 只编跑车（cd build && ctest -R mathjax_smoke 跑它）
```

## Step 1：引入 quickjs-ng

1. 源码包放 `3rdparty/quickjs-0.17.0.tar.gz`，解压出 `3rdparty/quickjs-0.17.0/`。
   **先把三样加进 `.gitignore`**（起点提交里没有，复刻实录 F1）：

   ```gitignore
   3rdparty/quickjs-0.17.0/
   node_modules/
   dist/
   ```

2. 解压后**先打补丁再 configure**。quickjs 的 CMakeLists 有两个 MSVC 编不过的
   东西，全部由 `scripts/patch_quickjs.ps1` 正则批改，跑一次即可：

   ```powershell
   .\scripts\patch_quickjs.ps1
   ```

   > **起点提交里的 `sb.ps1` 是旧内容**（当年给 qlitehtml 打 Qt 5.12 兼容
   > 补丁用的），必须先换成新版本——就是标准答案分支里这份（8 条正则 +
   > 无 BOM 写回），`git show <答案分支>:scripts/patch_quickjs.ps1 >
   > scripts/patch_quickjs.ps1` 直接取也可以（这份脚本早先叫根目录下的
   > `sb.ps1`，随 `sb` 目标退休改的名）。
   > 只跑旧脚本的后果：工具目标没被排除、qjs-libc 也没排除，全量构建时
   > `qjs CLI` 链接失败 + `stdatomic.h` 缺失（F4），两条一起炸。

   | 补什么 | 为什么 |
   |---|---|
   | 六个命令行工具（qjsc / qjs / api-test / lre-test / function_source / run-test262）`EXCLUDE_FROM_ALL` | qjs CLI 在 MSVC 下链接失败（16 个未解析符号） |
   | `add_qjs_libc_if_needed` 宏里的 `if(NOT QJS_BUILD_LIBC)` → 加 `AND NOT MSVC` | MSVC 下不链接 qjs-libc |
   | `add_library(qjs-libc STATIC quickjs-libc.c)` → 加 `EXCLUDE_FROM_ALL` | **qjs-libc 要 C11 的 `stdatomic.h`，MSVC 2019 没有**；不排除出 ALL，全量 `ninja` 编到它直接 `fatal error C1083: 无法打开包括文件: "stdatomic.h"`（复刻实录 F4） |

   > `scripts/patch_quickjs.ps1` 必须**纯 ASCII 注释**，且写回时不能带 BOM（脚本里用
   > `[System.IO.File]::WriteAllText` + `UTF8Encoding $false`）。Windows
   > PowerShell 5.1 会把无 BOM 的脚本按 ANSI 码页读，中文注释的字节会吞掉
   > 下一行——症状是 `$f` 莫名变成 null（复刻实录 F5）。
   >
   > quickjs 源码包已入库（`3rdparty/quickjs-0.17.0.tar.gz`，888KB），
   > `git checkout <答案分支> -- 3rdparty/quickjs-0.17.0.tar.gz` 直接取，
   > 省得重下——这是第三方包，不算抄答案。

3. 顶层 `CMakeLists.txt`，在 `add_subdirectory(3rdparty/qlitehtml)` 之后：

   ```cmake
   # quickjs-ng：关掉它的 install 规则，避免污染系统
   set(QJS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
   add_subdirectory(3rdparty/quickjs-0.17.0)
   ```

4. 先别接 GUI，加一个独立跑车（放 `tests/` 下，`tests/CMakeLists.txt` 声明）：

   ```cmake
   # 测试：QuickJS 跑车，验 JS 载荷不用起 GUI
   add_executable(mathjax_smoke mathjax_smoke.cpp)
   target_link_libraries(mathjax_smoke qjs)
   # MathJax 在 quickjs 里解析递归深（与 JS_SetMaxStackSize 配套）
   target_link_options(mathjax_smoke PRIVATE "/STACK:134217728")
   if(MSVC)
       # 源码是 UTF-8：不钉这个，中文注释的字节会被 GBK 误读吞掉行尾，
       # 表现为莫名其妙的 "undeclared identifier"（复刻实录 F2）
       target_compile_options(mathjax_smoke PRIVATE "/utf-8")
   endif()
   ```

   `tests/mathjax_smoke.cpp` 这一步先只写骨架：`JS_NewRuntime`/`JS_NewContext`/读文件/
   `JS_Eval`/打印。**它后面每踩一个坑都会补一段**，见 Step 5。

   > `/utf-8` 别等到 Step 7 才加：`mathjax_smoke.cpp` 里只要有中文注释，第一次编译
   > 就可能报 `error C2065: "code": 未声明的标识符`——那不是代码写错了，
   > 是注释末字节把下一行的声明吞进了注释（复刻实录 F2）。

> 踩坑：不 patch 就 configure，六个工具目标会被一起编，`qjs_exe` 在 MSVC
> 下链接失败导致整个构建停摆。

## Step 2：装 MathJax 与打包器

node 用本机现有的 v24（`D:\software\node-v24.15.0-win-x64`），工程根下：

```bat
npm install mathjax-full@3.2.1 esbuild@0.28.2
```

`package.json` 记得加 `"type": "module"`（esbuild 入口用 ESM 写）。

## Step 3：写打包入口 `tools/mathjax-entry.mjs`

**不要用 `mathjax-full/es5/*`**——那些是面向浏览器的组件包（引用
window/document），`es5/core` 更是只有调度核心，没有 TeX 输入器和 SVG 输出器。
正确姿势是从 `js/` 源码自己组，配上 `liteAdaptor`（纯 JS 极简 DOM，
专为 node/嵌入式设计）：

```js
import { mathjax } from 'mathjax-full/js/mathjax.js';
import { TeX } from 'mathjax-full/js/input/tex.js';
import { SVG } from 'mathjax-full/js/output/svg.js';
import { liteAdaptor } from 'mathjax-full/js/adaptors/liteAdaptor.js';
import { RegisterHTMLHandler } from 'mathjax-full/js/handlers/html.js';

const adaptor = liteAdaptor();
RegisterHTMLHandler(adaptor);
const tex = new TeX();
const svg = new SVG({ fontCache: 'none' });
const doc = mathjax.document('', { InputJax: tex, OutputJax: svg });

globalThis.MathJaxAdapter = {
  // 返回独立可渲染的 <svg> 字符串；display=true 按独立公式排版
  convert: function (texCode, display) {
    const node = doc.convert(texCode, {
      display: !!display,
      em: 16, ex: 8, containerWidth: 1280,
    });
    return adaptor.outerHTML(node);
  },
};
```

三个关键点：

- `fontCache: 'none'`：global 模式会把字形路径放第一段 SVG 里，单独渲染后面
  几段会缺字。每段自带 `<defs>` 才是自包含的
- `em: 16, ex: 8`：排版度量基准，后面 C++ 把 SVG 的 ex 尺寸换算成像素时用的
  就是同一组数
- 挂 `globalThis.MathJaxAdapter`：QuickJS 侧 `JS_Eval(..., JS_EVAL_TYPE_GLOBAL)`
  之后直接可用

打包（两条 define 都不能少）：

```bat
node_modules\.bin\esbuild tools\mathjax-entry.mjs --bundle --minify --format=iife ^
  --define:process.env.NODE_ENV=\"production\" ^
  --define:PACKAGE_VERSION=\"3.2.1\" ^
  --outfile=dist\mathjax-svg.min.js
```

> 踩坑：`--define:PACKAGE_VERSION` 不能省。mathjax-full 的版本探测写着
> `eval("require")` 去读 package.json，运行时没有 require 就炸
> （`Cannot find module 'D:\package.json'`）。钉死常量后它走另一条分支。

## Step 4：node 里先验证，别急着上 C++

用文件写测试（**别在 `node -e "..."` 里内联 `\frac`**——bash 双引号吃一层
反斜杠，`\f` 变成 JS 换页符，渲染器会对着换页符报无关的错，白绕一小时）：

```js
// temp/test_mathjax.cjs
const fs = require('fs'), vm = require('vm');
vm.runInThisContext(fs.readFileSync('dist/mathjax-svg.min.js', 'utf8'));
// iife 的 var 在 require 作用域里不进 global，所以要 vm 注入，不能 require
for (const [tex, display] of [
  ['E = mc^2', false], ['\\frac{a}{b}', true],
  ['\\int_{-\\infty}^{\\infty} e^{-x^2}\\,dx', true],
  ['\\sum_{i=1}^{n} x_i^2', true], ['\\sqrt[3]{x+1}', false],
]) {
  const svg = MathJaxAdapter.convert(tex, display);
  console.log(svg.length, 'B :', tex);
}
```

五条全出 SVG（1.6KB～6KB）才进下一步。

## Step 5：C++ 跑车 `mathjax_smoke`（QuickJS 里过一遍）

给 `tests/mathjax_smoke.cpp` 补三块：

1. **console shim**：QuickJS 没有 console，MathJax 内部日志会炸，先注入一个
   `console.log/error`。

   > **log 与 error 必须各建一个函数对象**（两次 `JS_NewCFunction`）：
   > `JS_DefinePropertyValueStr` 会**消费（偷走）**值的引用，同一个对象挂两次
   > 就是引用计数下溢，quickjs 直接断言
   > `Assertion failed: JS_REF_COUNT(p) > 0, file .../quickjs.c, line 7365`
   > （复刻实录 F3）。
2. **栈，两个都要放大**：

   ```cpp
   JS_SetMaxStackSize(rt, 64u << 20);   // quickjs 侧配额
   ```

   ```cmake
   # tests/CMakeLists.txt，mathjax_smoke 目标上
   target_link_options(mathjax_smoke PRIVATE "/STACK:134217728")   // 真线程栈
   ```

   > 踩坑：MathJax 解析递归深。只放 quickjs 配额不放线程栈（或反之），
   > convert 直接硬崩无输出——JS 异常和原生崩溃在这时分不清，convert 调用
   > 先包一层 `try/catch` 让 JS 错误以字符串形式回来。

3. **加载与调用**：

   ```cpp
   std::string code = read_file("dist/mathjax-svg.min.js");
   JS_Eval(ctx, code.c_str(), code.size(), "mathjax", JS_EVAL_TYPE_GLOBAL);
   // convert 用 try/catch 包过的版本
   ```

4. **遍历全局对象时的两条命**：

   ```cpp
   JS_FreeAtom(ctx, enumerable[i].atom);   // atom 是引用计数的
   // ...
   js_free(ctx, enumerable);               // 不是 free()！
   ```

   > 踩坑：`JS_GetOwnPropertyNames` 的数组是 **quickjs 内部堆**分配的，
   > 用 CRT 的 `free()` 就是 `_CrtIsValidHeapPointer` 断言弹窗
   > （debug_heap.cpp:904）——两个堆实例对不上账。

验收：`mathjax_smoke.exe`（或 `cd build && ctest -R mathjax_smoke -V`）打印
`Found Global Object: MathJaxAdapter` 并输出一段
`<svg width="8.699ex" ...`。注意输出**包在 `<mjx-container>` 里**，记下这点，
Step 6 要用。

## Step 6：接进 GUI

`src/mainwindow.h`：前向声明 + 成员 + 方法（qlitehtml 类型不进公开头的同一套
纪律）：

```cpp
struct JSRuntime;
struct JSContext;
// ...
    bool InitMathJax();
    QString ConvertTexToImgTag(const QString& tex, bool display);
    QByteArray FormulaPng(const QUrl& url);
    JSRuntime* mj_rt_ = nullptr;
    JSContext* mj_ctx_ = nullptr;
// ~MainWindow() 里 JS_FreeContext + JS_FreeRuntime
```

`src/mainwindow.cpp` 三个方法（全文见标准答案）：

- `InitMathJax()`：读 `dist/mathjax-svg.min.js`（路径从 `applicationDirPath()`
  推导，别写死绝对路径——worktree/挪家都靠它）→ `JS_Eval`（带
  `JS_SetMaxStackSize`，异常时打日志）
- `ConvertTexToImgTag()`：
  1. tex 进单引号 JS 字符串前，反斜杠和引号**先翻倍**（`\frac` 的 `\` 不翻倍
     就是 Step 4 的换页符惨案）
  2. 调用包 `try/catch`，返回值认 `SVGOK:` 前缀
  3. **剥 `<mjx-container>` 壳**：QSvgRenderer 只认 svg 根元素
  4. 尺寸换算：SVG 的 `width="8.699ex"` × 8px（ex=8，与 Step 3 的约定一致）
  5. **2x 超采样**：QImage 按 2 倍画，`<img>` 按 1 倍摆，曲线是锐的
  6. PNG → base64 → **data URI 内联**：

     ```html
     <img src="data:image/png;base64,..." width="70" height="17">
     ```

- `FormulaPng()`：从 data URI 解出 PNG 字节

资源回调里加一条分发（本地文件的分支保留）：

```cpp
preview_->setResourceHandler([this](const QUrl& url) -> QByteArray {
    const QByteArray png = FormulaPng(url);
    if (!png.isEmpty()) return png;
    /* 原有本地文件分支 */
});
```

> 踩坑 A：公式 URL 用 `mj://0` 这种自定义 scheme 会被 litehtml 当主机名做
> 浏览器式扩展（`0` → `0.0.0.0`），回调收到的串对不上号。data URI 根本不过
> URL 解析，天然免疫。
>
> 踩坑 B：为什么不学 KaTeX 那样排 HTML？实测见 `temp/katex_after3.png`——
> litehtml 的 CSS2.1 表格引擎给不出 KaTeX vlist 需要的浏览器语义，
> 分式撑满整行、上标消失。**排版交给 JS 引擎，litehtml 只画图**，这是整个
> 方案的立脚点。

## Step 7：CMake 收尾

`src/CMakeLists.txt`：

```cmake
# 公式走 SVG→PNG，需要 QtSvg（QSvgRenderer）
find_package(Qt5 5.12 QUIET COMPONENTS Svg)
if(NOT Qt5Svg_FOUND)
    message(FATAL_ERROR "Qt5Svg 没找到：公式渲染依赖 QSvgRenderer")
endif()
target_link_libraries(${TARGET_NAME} PRIVATE Qt5::Svg)
# GUI 直接调 quickjs 的 C API：不链它会 fatal error C1083: "quickjs.h"
# 找不到（复刻实录 F6）
target_link_libraries(${TARGET_NAME} PRIVATE qjs)

if(MSVC)
    # 源文件是 UTF-8；不钉这个 MSVC 按 GBK 读，UTF-8 字节会被误配对吞掉
    # 代码字符（C2059 一类的假语法错误就是这么来的）
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc /utf-8)
    # MathJax 在 quickjs 里解析递归深（与 JS_SetMaxStackSize 配套）
    target_link_options(${TARGET_NAME} PRIVATE "/STACK:134217728")
endif()
```

> 踩坑：`/utf-8` 不加，UTF-8 源文件里中文注释的字节在 GBK 视角下会"吞掉"
> 行尾的 `)` 或换行，报 C2059 这种假语法错误，跟真 bug 隔离不开。

---

## Linux / WSL 验证（实测通过）

环境：WSL2 Ubuntu 20.04 + WSLg，**系统 Qt 5.12.8**、cmake 3.31.12（`~/env_software` 那份，
不用系统 cmake）、ninja 1.10.0、g++ 9.4。这条路对 **Linux + Qt 5.12** 都成立。

```bash
mkdir -p ~/works && rsync -a --exclude build/ --exclude node_modules/ \
    /mnt/d/trae_workspace/qt-md-renderer-demo/ ~/works/qt-md-renderer-demo/
cd ~/works/qt-md-renderer-demo
# local.cmake 留成空文件即可（顶层 include 它，删了会报错）：
#   Linux 上系统 Qt 在 /usr/lib/x86_64-linux-gnu/cmake/Qt5，find_package 自己找得到
~/env_software/cmake-3.31.12-linux-x86_64/bin/cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug
ninja -C build
./build/bin/mathjax_smoke      # 冒烟：应打印 SVG:<mjx-container … width="8.699ex"
./build/bin/qt-md-renderer-demo   # WSLg 窗口里应看到四条公式
```

实测结果：`mathjax_smoke` 输出的 SVG 与 Windows 侧**逐字节一致**；GUI 四条公式（行内/分式/积分/求和）
排版正常。三个要点：

- **`/STACK` 必须包在 `if(MSVC)` 里**（踩坑总表 F9）：它是 MSVC 链接器选项，
  直接喂给 GCC 会 `c++: error: /STACK:134217728: No such file or directory`。
- **`local.cmake` 留空但不能删**：顶层 `include(local.cmake)` 没有 `OPTIONAL`，
  文件不存在直接配置失败；而里面那份 Windows Qt 路径（`D:/Qt/…`）在 Linux 上是废的。
- **qlitehtml 的 Qt 5.12 守卫是入库的**（`3rdparty/qlitehtml` 256 个文件都在 git 里，
  `container_qpainter.cpp` 里 `QT_VERSION_CHECK` 那三处已提交），所以 Qt 5.12.8 开箱即编。
  注意它和 quickjs 的处境不同：**quickjs 是"解压即弃 + 脚本打补丁"，qlitehtml 是"源码入库"**。

> 截图取证的小坑：WSLg 的窗口标题会被加上发行版后缀（实际是
> `Qt Markdown Renderer (Ubuntu)`），按精确标题 `FindWindow` 找不到，得前缀匹配；
> 且 RAIL 窗口 `GetWindowRect` 拿到的是 1×1，取不到尺寸时改抓整屏再裁。

干净重编（`rm -rf build` → configure → ninja）实测 **零告警、零错误**（GCC 9.4），
`build.log` 里唯一含 "error" 的行是 gumbo 的 `error.c` 文件名。

**管线接入也在 WSL 上复验过**（同步 `src/formula_renderer.*`、`markdown_to_html.cpp`、
`mainwindow.*` 后重编 + 跑 `temp/formula_test.md`）：行内公式、独立公式居中、
求和/上下标，以及"代码块里的 `$1`、行内代码 `$HOME`、正文钱数不配对、`\$` 转义"
这些边界项，表现与 Windows 逐项一致。

### 双工作区（Windows + WSL 各一份）的三条注意事项

| # | 事项 | 说明 |
|---|---|---|
| F10 | `local.cmake` 是**平台专属**的，别互相覆盖 | Windows 那份指 `D:/Qt/Qt5.12.12/...`，Linux 那份留空（系统 Qt 自己找得到）。它是入库文件，所以两边各自保留各自的版本、不要 commit 对方那版 |
| F11 | 拷代码到 Linux **别带 `node_modules/`** | esbuild 是按平台分发的二进制，Windows 那份在 Linux 上跑不起来；到那边 `npm install` 重装。`dist/mathjax-svg.min.js` 是纯 JS，可以直接带过来（不想在 WSL 装 node 的话就靠它） |
| F12 | 跨侧比对别被**行尾**骗了 | Windows 工作区是 CRLF、WSL 侧是 LF，直接 `diff` 会满屏假差异；用 `diff --strip-trailing-cr`，或让 git 归一化后比对 |

拷代码的推荐姿势（排除构建产物与平台二进制）：

```bash
rsync -a --exclude build/ --exclude node_modules/ /mnt/d/trae_workspace/qt-md-renderer-demo/ \
    ~/works/qt-md-renderer-demo/
```

---

## 踩坑总表（两轮实现汇总，按踩到的顺序）

编号是记录用的，`F*` = 复刻时新增，`P*` = 首次实现时踩的。

### 方案层

| # | 症状 | 根因 | 修法 |
|---|---|---|---|
| P8 | KaTeX 路线的公式排版崩塌：分式分数线拉成通栏宽、上标消失、求和号变形 | litehtml 的 CSS2.1 表格引擎给不出 KaTeX vlist 需要的浏览器语义（`inline-table` 不收缩到内容宽、负 `top` 相对偏移 + `pstrut` 裁剪失效） | **换方案**：排版交给 JS 引擎（MathJax 出 SVG），litehtml 只画图。这是整条管线的立脚点，别再试图调 CSS |

### 打包层（Step 2–3）

| # | 症状 | 根因 | 修法 |
|---|---|---|---|
| P1 | QuickJS 里 `MathJaxAdapter` 不存在 / 函数不是函数 | 入口选了 `mathjax-full/es5/core`：它只是调度核心，没有 TeX 输入器、SVG 输出器、适配器（产物 219KB，全套应 ≥1MB） | 从 `mathjax-full/js/*` 源码组 TeX + SVG + `liteAdaptor`，esbuild 打包（产物 1.5MB） |
| P2 | `Cannot find module 'D:\package.json'` | mathjax-full 的版本探测用 `eval("require")` 读 package.json，运行时没有 require | 打包时 `--define:PACKAGE_VERSION='"3.2.1"'` |
| P3 | `\frac` 报"读不到 null 的属性 4"，而 `x^2`/`\sqrt`/`\sum` 都正常 | 报错里显示的输入是 `rac12` —— bash 双引号把内联测试的 `\f` 吃成了换页符，渲染器对着控制字符报错 | node 验证脚本**写文件跑**，别在 `node -e "..."` 里内联 TeX |
| P9 | 第一段之后所有 SVG 缺字 | `fontCache: 'global'` 把字形路径只放在第一段 SVG 的 `<defs>` 里 | `new SVG({ fontCache: 'none' })`，每段自带 |

### C++ / QuickJS 层（Step 5）

| # | 症状 | 根因 | 修法 |
|---|---|---|---|
| F3 | `Assertion failed: JS_REF_COUNT(p) > 0, quickjs.c:7365` | `JS_DefinePropertyValueStr` **消费**值的引用；把同一个函数对象同时挂给 `console.log` 和 `console.error` 就是引用计数下溢 | 两次 `JS_NewCFunction`，各挂各的 |
| P4 | `Debug Assertion Failed: _CrtIsValidHeapPointer(block)`（debug_heap.cpp:904），进程卡在弹窗 | `JS_GetOwnPropertyNames` 返回的数组是 **quickjs 内部堆**分配，`free()` 走的是另一个 CRT 堆实例 | `js_free(ctx, enumerable)`；顺手 `JS_FreeAtom(ctx, ...)` 每个 atom |
| P5 | 调用 `convert` 直接硬崩、无输出、无异常（JS 异常与原生崩溃分不清） | MathJax 解析递归深；quickjs 默认栈配额贴着线程栈 | `JS_SetMaxStackSize(rt, 64MB)` **和** 链接选项 `/STACK:134217728`，两边都要；调用处包 `try/catch` 让 JS 错误变成字符串 |
| F13 | 独立公式（`$$` 单独成行的块形式）静默落到"显示原文"兜底，行内公式却正常 | 块形式跨行收集，tex 里带着换行；拼进 JS 单引号字符串前只转义了 `\` 和 `'`，裸换行让 JS 直接语法错误 | tex 先进 JS 字符串前 `trimmed()`，并把 `\n`/`\r` 一并转义（`\n` → `\\n`）——**任何进 JS 字符串的文本都要过这一道** |
| P10 | 修好一个坑，下一个 `JS_Eval` 又崩 | 同一类：栈、引用计数、堆不匹配 | 每个坑单独复现一次再修，别一次改多处（否则说不清是哪个生效了） |

### GUI 接入层（Step 6–7）

| # | 症状 | 根因 | 修法 |
|---|---|---|---|
| P6 | `[svg-invalid]`，QSvgRenderer 拒绝 | MathJax 输出外面包着 `<mjx-container>`，QSvgRenderer 只认 `<svg>` 根元素 | 抠出 `<svg …>…</svg>` 再画 |
| P7 | PNG 都在、资源回调也收到了请求，图就是不显示；回调收到的是 `mj://0.0.0.0` | litehtml 把自定义 scheme 的 `mj://0` 当主机名做浏览器式扩展 | 改用 **data URI** 内联，完全不经 URL 解析 |
| F6 | `fatal error C1083: "quickjs.h" 找不到` | Qt 目标没链 `qjs`，include 路径没继承过来 | `target_link_libraries(${TARGET_NAME} PRIVATE qjs)` |
| F7 | `error C2027: 使用了未定义类型 "QDebug"` + `operator<<` 无匹配 | 用了 `qWarning()` 但没包含 `<QDebug>` | `#include <QDebug>` |
| F8 | 链接 `bin/qt-md-renderer-demo.exe` FAILED，没有任何编译错误 | 上一轮验证时把程序开着，exe 被占用 | 编译前先关掉在跑的程序 |

### 构建 / 脚本层（Step 0–1）

| # | 症状 | 根因 | 修法 |
|---|---|---|---|
| F4 | 全量 `ninja` → `fatal error C1083: "stdatomic.h"` | `qjs-libc` 要 C11 atomics，MSVC 2019 没有；它在 ALL 里 | patch_quickjs.ps1 补两处：`add_qjs_libc_if_needed` 宏的条件加 `AND NOT MSVC`、`add_library(qjs-libc …)` 加 `EXCLUDE_FROM_ALL` |
| F5 | PowerShell 报 `Get-Content: 无法绑定参数 Path，因为该参数是空值`，`$f` 莫名其妙是 null | PS 5.1 把**无 BOM 的 UTF-8 脚本**按 ANSI（GBK）解码，中文注释的尾字节吞掉了下一行的赋值 | patch_quickjs.ps1 纯 ASCII 注释；写回用 `[IO.File]::WriteAllText(..., UTF8Encoding $false)`，`Set-Content -Encoding UTF8` 会带 BOM |
| F1 | `git status` 里冒出整个解压目录 + `node_modules/` + `dist/` | 起点提交的 `.gitignore` 没有这些条目 | 三行加进 `.gitignore`（Step 1 第 1 条） |
| F2 | `error C2065: "code": 未声明的标识符`，代码看着完全正确 | 源文件是 UTF-8，MSVC 按 GBK 读，中文注释末字节把下一行的声明吞进注释（和 F5 同一个病，换了个宿主） | 该编译目标加 `/utf-8`——**mathjax_smoke 与 Qt 目标都要**，别只加后者 |
| F9 | Linux 下 `c++: error: /STACK:134217728: No such file or directory`，链接失败 | `target_link_options(mathjax_smoke PRIVATE "/STACK:…")` 没包在 `if(MSVC)` 里，MSVC 链接选项被喂给了 g++ | 挪进 `if(MSVC)`；Linux 靠默认栈即可（MathJax 递归在 Linux 上没触发过栈问题） |
| F10 | 两边工作区互相覆盖了 `local.cmake`，另一侧配置失败 | 它是**平台专属**的入库文件：Windows 指 `D:/Qt/...`、Linux 留空，没有哪一版对两边都对 | 两边各留各的，别 commit 对方那版（见「双工作区的三条注意事项」） |
| F11 | Linux 上 `npm run` / esbuild 报 **Exec format error** | 直接拷了 `node_modules/`：esbuild 是按平台分发的二进制，Windows 那份在 Linux 上跑不了 | 拷代码时排除 `node_modules/`，到那边 `npm install`；`dist/` 是纯 JS 可直接带 |
| F12 | 跨侧 `diff` 满屏假差异，看着像改了一堆 | Windows 工作区 CRLF、WSL 侧 LF | 用 `diff --strip-trailing-cr`；或让 git 归一化后比对 |

> F2 / F5 是同一个根因的两种发作：**非 ASCII 的 UTF-8 内容遇到按 ANSI 解码的
> 工具链**。本仓的规矩是脚本一律不写中文注释 / 中文串，编译器目标一律 `/utf-8`。

---

## 验证清单

跑起来（编辑器侧的 `refreshPreview` 临时注释，让测试页停住）：

- [ ] `E = mc^2` 行内：斜体 + 上标，尺寸约 70×17
- [ ] `\frac{a}{b}`：a 上 b 下，横线居中（KaTeX 路线下这条是通栏塌的）
- [ ] `\int_{-\infty}^{\infty} e^{-x^2}\,dx`：上下限齐
- [ ] `\sum_{i=1}^{n} x_i^2`：求和号 + 上下标
- [ ] PNG 边缘锐利（2x 采样生效）

## 接进 markdown 管线（已落地）

公式不再只活在临时测试块里，它现在是 `markdownToHtml()` 的一部分：

```text
md 源码
  ├─ extractMath()      ← md4c **之前**：把公式抽成占位符 @@mathjax-N@@
  │                       跳过围栏代码块、行内代码与 \$（那里的 $ 是字面量）
  ↓
md4c（GFM）
  ↓
postProcessCodeBlocks()  ← 代码块语法高亮
  ↓
restoreMath()            ← md4c **之后**：占位符 → 渲染好的 <img>
```

几个设计要点：

- **为什么抽成占位符、在两头做**：$ 在代码里是字面量、在正文里是公式，只有
  源码阶段分得清；对成品 HTML 正则会把代码块里的 `$1` 也当公式。占位符用纯
  ASCII 的 `@@mathjax-N@@`（写成 `<img>` 反而会被 md4c 转义/重排）
- **行内 `$…$` 的配对规则**（照 Pandoc 的口径）：开 `$` 后不能是空白，闭 `$`
  前不能是空白、后面不能跟数字——这样"价格在 $5 到 $10 之间"不会被误配对
- **独立公式两种写法都支持**：同一行 `$$…$$`，以及 `$$` 单独成行的块形式
  （块形式会跨行收集，tex 里的换行在进 JS 之前必须转义，见踩坑总表 F13）
- **懒加载 + 缓存**：`formulaToImgTag()` 内部首次调用才加载 1.5MB 的 bundle；
  结果按 (tex, display) 缓存，**失败也缓存空串**——同一个坏公式不会每帧重试
- **失败兜底**：渲染不出来就在原位显示红色原文（`<code class="math-src">`），
  一条坏公式不会吞掉整页
- **独立公式居中**：`.math-display { display: block; margin: 10px auto; }`
- **代码位置**：`src/formula_renderer.{h,cpp}`（JS 运行时 + 缓存 + SVG→PNG），
  `MainWindow` 不再持有 JS 运行时

KaTeX 那套（`3rdparty/katex`、`addApplicationFont`、字体注册）在本方案下已无
用处，留档在 `temp/` 未入库。
