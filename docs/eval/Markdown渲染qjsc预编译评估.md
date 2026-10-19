# Markdown 渲染：qjsc 预编译字节码评估

状态：已完成（2026-10-19）。结论：**采纳字节码路线**，待实施。

## 一、要回答的问题

JS 载荷（highlight 66KB / MathJax 1.6MB / mermaid 3.5MB 等 5 份）目前以源码
进 qrc，首次加载在 QuickJS 里 `JS_Eval` 现场解析，冷启动卡顿明显。评估改用
qjsc 构建期预编译字节码是否值得，三个口径：

1. 冷加载时间（同基线 A/B，非跨版本对比）；
2. 二进制体积；
3. 上游 JS 包升级的跟进成本。

## 二、实验方法

- worktree：`D:/trae_workspace/qt-cmake-base-env-eval`，分支 `eval-qjsc`
  （提交 `dd989a5`），基线 master `1c7b4b3`；
- 两个 CMake 开关（只存在于评估分支，**不随结论入库**）：
  - `MARKDOWN_JS_BYTECODE=ON/OFF`：qrc 源码 vs 构建期 qjsc 字节码；
  - `MARKDOWN_QRC_COMPRESS=ON/OFF`：qrc 显式压缩（验证默认行为）；
- 时间口径：`bench_cold_load`（评估分支自带）打印三引擎
  `MarkdownEngineStatus.first_load_ms` + 首次 `ApplyDocument`（样张含公式、
  代码块、mermaid 各一）；offscreen、磁盘缓存指向空临时目录（防磁盘命中
  跳过加载）、每变体跑 5 次取中位数；
- 体积口径：`md_viewer.exe`，`/INCREMENTAL:NO` 全链接（增量链接会让 exe
  只增不缩，首轮数据因此作废过一次，见 §六）。

## 三、改了什么（eval 分支 `dd989a5`）

| 文件 | 改动 |
|---|---|
| `src/markdown/CMakeLists.txt` | 两个评估开关；qrc 改模板生成；字节码变体下增加 qjsc 生成段（`-C -s -s -N`，五份 JS → 五个 C 数组），只认 in-tree `qjsc` target，无 `find_program` 回退 |
| `src/markdown/markdown_res.qrc.in` | qrc 模板：compress 属性与绝对路径由 CMake 注入；字节码变体下 JS 条目整体剔除（只留 CSS） |
| `src/markdown/markdown_bytecode.h` | 五个 `extern "C"` 字节数组声明 |
| `code_highlighter.cpp` / `formula_renderer.cpp` / `mermaid_renderer.cpp` | `EnsureLoaded` 按 `#ifdef MARKDOWN_JS_BYTECODE` 双路径：字节码走 `JS_ReadObject(JS_READ_OBJ_BYTECODE)` + `JS_EvalFunction`；qrc 路径逐字节保持 master 现状 |
| `src/markdown/render_cache.cpp` | `PayloadHash` 双路径：字节码变体直接哈希 C 数组（载荷版本维度保留）；qrc 变体维持读 qrc 字节 |
| `tests/markdown/bench_cold_load.cpp` | 冷加载基准（自带 main，非 gtest） |

## 四、数据

### 4.1 冷加载时间（5 次中位数，x86 relwithdebinfo，offscreen）

| 指标 | A 现状（qrc 源码） | B 字节码 | Δ |
|---|---|---|---|
| highlight 冷加载 | 15 ms | 5 ms | -67% |
| MathJax 冷加载 | 171 ms | 31 ms | -82% |
| Mermaid 冷加载 | 732 ms | 85 ms | **-88%** |
| 首次 ApplyDocument（含三者 + 排版） | 1566 ms | 669 ms | **-57%** |

正确性：B 下 `ctest -L MARKDOWN` 17/17 通过（16 项测试 + bench）。

### 4.2 二进制体积（md_viewer.exe，/INCREMENTAL:NO）

| 变体 | 大小 | 说明 |
|---|---|---|
| A 现状 | 5 570 048 B | rcc **默认已压缩**（实测确认） |
| B 字节码 | 8 470 016 B | +2.9 MB（+52%） |
| C qrc + 显式 compress | 5 570 048 B | 与 A 逐字节相同，无增益 |

载荷本体：JS 源码 5 308 243 B（qrc 压缩后远小于此）vs 字节码 5 617 262 B
（`md_hl` 130 640 + `md_mj` 1 517 232 + `md_mm` 3 929 115 + `md_ds` 36 422 +
`md_pb` 3 853，不可压缩，1:1 进 exe）。

### 4.3 升级演练

修改 `highlight.min.js`（追加一行注释）→ 重建 → `qjsc:` 步骤因 `DEPENDS`
自动重跑（`md_hl.c` 时间戳 10:23 → 10:31）→ `test_code_highlighter` 通过 →
还原。升级上游 JS 包无需任何人工登记，与 qrc 路线负担相同。

## 五、结论

**采纳字节码路线。** 首次带图文档的冷加载从 1.57 s 降到 0.67 s，代价是
exe +2.9 MB；qrc 显式压缩已无增益（默认行为），"压缩省体积"路线不存在
权衡空间。升级流程无额外负担。

实施要点（落到 master 时）：

1. 基于本评估分支的改动手工重放，**去掉评估开关**，只留字节码单路径；
2. 保留"只认 in-tree `qjsc` target"的约束：系统 PATH 里异版本 qjsc 产出
   的字节码 BC_VERSION 不匹配，会拖到运行时才永久 FailLoad，必须
   configure 期拦截；
3. `PayloadHash` 换成哈希字节码数组（跟随实施，否则持久缓存丢载荷维度）；
4. `x64` 与 WSL 口径未跑（本轮只有 x86）；WSL 侧字节码由该构建树自己的
   qjsc 生成，机制上无障碍，实施后按惯例过一遍；
5. "载荷压缩复量"todo 可划掉（默认已压缩，显式开关无增益）。

## 六、实验里踩的坑（复现时注意）

- **qrc 双份**：字节码变体的 qrc 模板若不剔除 JS 条目，字节码与源码会
  同时进 exe（首轮 B 的 exe 因此虚高到 14.5 MB）；
- **增量链接**：RelWithDebinfo 带 `/DEBUG` → MSVC 默认增量链接，exe 只增
  不缩且体积不可信；三个变体必须统一 `/INCREMENTAL:NO` 后再比；
- **磁盘缓存**：冷加载测量前必须清持久缓存（磁盘命中会整段跳过引擎
  加载，`first_load_ms` 直接为 0）。

## 七、复现步骤

```bash
git worktree add ../qt-cmake-base-env-eval -b eval-qjsc <master 提交>
cp local.cmake ../qt-cmake-base-env-eval/          # 不入库，必拷

cd ../qt-cmake-base-env-eval
# A（现状）
scripts/build_ninja.bat -t relwithdebinfo -a x86 -m bench_cold_load
# B（字节码）
3rdparty/cmake/bin/cmake.exe -B build/ninja-relwithdebinfo-x86 -D MARKDOWN_JS_BYTECODE=ON
scripts/build_ninja.bat -t relwithdebinfo -a x86 -m bench_cold_load
# 每个 B/A 构建后在构建目录跑 5 次：
3rdparty/cmake/bin/ctest.exe -R bench_cold_load -V | findstr BENCH
```

注：评估分支的开关与 bench 不随结论入库；master 落地时只保留字节码
单路径 + 本文档。
