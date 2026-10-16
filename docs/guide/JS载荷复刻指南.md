# JS 载荷复刻指南

`src/markdown/resources/` 下的几份 JS 都是**生成物**。运行与部署不碰 node——它们
已进 qrc，Windows 与 Linux 共用同一份；只有升级上游时才重建。本页把复刻环境所
需的东西凑齐：上游包与版本、重建命令、产物校验值。入口文件头部各有一份精简版，
本页是完整版（含 Mammoth 这条还没入基座的线）。

## 环境

| 组件 | 版本 | 来源 |
| --- | --- | --- |
| Node.js | 24.15.0 | `aa-sources/node-v24.15.0-win-x64.zip` |
| esbuild | 0.28.2 | `aa-sources/esbuild-0.28.2.tgz` + 平台二进制 `esbuild-win32-x64-0.28.2.tgz` |
| mathjax-full | 3.2.1 | `aa-sources/mathjax-full-3.2.1.tgz` |
| highlight.js | 11.12.0 | `aa-sources/highlight.js-11.12.0.tgz` |
| mammoth | 1.13.0 | `aa-sources/mammoth-1.13.0.tgz`（未入基座，见下） |
| mermaidx | 0.9.5 | `aa-sources/mermaidx-0.9.5.tar.gz`（pip 包，只当素材） |

npm 包不必常驻，找个空目录装一份即可，两种装法等价：

```bash
# 联网：从 registry（传递依赖随之进来）
npm install mathjax-full@3.2.1 highlight.js@11.12.0 esbuild@0.28.2

# 离线：用归档件（<AA> 指 aa-sources；esbuild 的平台二进制要单独装）
npm install <AA>/mathjax-full-3.2.1.tgz <AA>/highlight.js-11.12.0.tgz \
    <AA>/esbuild-0.28.2.tgz <AA>/esbuild-win32-x64-0.28.2.tgz
```

归档件核对值（`md5sum`）：

| 文件 | 字节 | md5 |
| --- | --- | --- |
| `mathjax-full-3.2.1.tgz` | 8530977 | `8ddf0a09510682f9d1db2e5fc235332b` |
| `highlight.js-11.12.0.tgz` | 853791 | `0f1a956ba1862744de67bf6e373faf69` |
| `esbuild-0.28.2.tgz` | 34207 | `443d5f73d7e04431c30b22c9329f4fa2` |
| `esbuild-win32-x64-0.28.2.tgz` | 4829680 | `53ce9065009f89dd203454e189df0a51` |
| `mammoth-1.13.0.tgz` | 438003 | `078982da8f3e5452187f5fdacb3e0b47` |

归档的是**顶层包**：传递依赖（mathjax-full 的 speech-rule-engine 系、mammoth 的
jszip 系）不在其中，`npm install` 时照常从 registry 取；`--offline` 装法靠本机
npm cache 供这些传递依赖，换台干净机器要联网补一次。

下文命令里的 `<NPM_DIR>` 指这个目录，命令按 bash 写（Git Bash 下验证过）。
实测（2026-10-16）：从归档 tgz 离线装出 `<NPM_DIR>` 后重建，MathJax、highlight
两份与本仓**逐字节一致**；Mammoth 那份与 demo 的 `dist/mammoth.min.js` 也一致。

## 载荷清单

| 载荷 | 上游 | 入口（仓库内） | 字节 | md5 |
| --- | --- | --- | --- | --- |
| `mathjax-svg.min.js` | mathjax-full 3.2.1 | `mathjax_svg_entry.mjs` | 1618089 | `bfdd6f73560f2e17c09b250cb84aa24c` |
| `highlight.min.js` | highlight.js 11.12.0 | `highlight_entry.js` | 66390 | `439dc3414567e31c1bc58f8172643a04` |
| `mermaid.js` | mermaid 11.16.0（官方 bundle，随 mermaidx） | — | 3565102 | `1dd12fadbfa258c3ac600e9e7f1d6a3d` |
| `dom_shim.js` | mermaidx 0.9.5 | — | 53972 | `d8d380c55772e2868cdd075d4c42a5c5` |
| `path_bbox.js` | mermaidx 0.9.5（`path_bbox.py` 里的字面量） | — | 4690 | `8f82e27f69973c1e31fd488dfa67435e` |

mermaid 的版本看 bundle 里的 `version:"11.x.y"`，与 mermaidx 的版本号互相独立。

## MathJax

在仓库根目录执行：

```bash
<NPM_DIR>/node_modules/.bin/esbuild src/markdown/resources/mathjax_svg_entry.mjs \
  --bundle --minify --format=iife \
  --alias:mathjax-full=<NPM_DIR>/node_modules/mathjax-full \
  --define:process.env.NODE_ENV='"production"' \
  --define:PACKAGE_VERSION='"3.2.1"' \
  --outfile=src/markdown/resources/mathjax-svg.min.js
```

两个 `--define` 都不能省：mathjax-full 的版本探测写着 `eval("require")` 去读
package.json，而 QuickJS 里没有 `require`。入口里为什么不用 `es5/` 组件包、
`fontCache` 为什么取 `none`，见入口文件头部。

实测（2026-10-16）：按此命令重建，与入库那份**逐字节一致**。

## highlight.js

在仓库根目录执行：

```bash
<NPM_DIR>/node_modules/.bin/esbuild src/markdown/resources/highlight_entry.js \
  --bundle --format=iife --minify \
  --alias:highlight.js=<NPM_DIR>/node_modules/highlight.js \
  --outfile=src/markdown/resources/highlight.min.js
```

**`--alias` 不能省**：入口住在仓库里，这里没有 `node_modules`，不指过去就解析
不到包。把入口拷到 `node_modules` 旁边也能建，但**产物字节会变**——入口所在目录
有没有 `package.json`（`type: module`）会改变 esbuild 的 CJS 互操作包装，实测差
20 字节（66410 vs 66390），行为相同。要复现入库那份，照上面原样跑。

实测（2026-10-16）：按此命令重建，与入库那份**逐字节一致**。

## mermaid（mermaidx）

不经过 esbuild，三份都是逐字节取来的，重建与校验都在脚本里：

```bash
python src/markdown/resources/extract_mermaid_payload.py <mermaidx 目录> --check
```

`--check` 只对哈希、不改文件；确认升级再重跑（去掉 `--check`）覆盖三份载荷。
换行、`-text` 属性这些坑写在脚本头部注释里（哈希要跨平台可比，就别让 git 动
字节）。

## Mammoth（未入基座，配方暂存 demo）

docx 那条管线还在 `qt-md-renderer-demo` 试验田里，这里的配方备查，将来入库时按
highlight.js 的形态办（入口进 `resources/`、载荷进 qrc、配方写进入口头部）：

| 项 | 值 |
| --- | --- |
| 上游 | npm `mammoth@1.13.0`（`aa-sources/mammoth-1.13.0.tgz`；入口 import 的是 `mammoth/mammoth.browser.js`） |
| 入口 | demo `tools/mammoth-entry.js`（补 QuickJS 缺的 `setTimeout` / `TextDecoder`） |
| 命令 | `npx esbuild tools/mammoth-entry.js --bundle --format=iife --minify --outfile=dist/mammoth.min.js` |
| 产物 | demo `dist/mammoth.min.js`，309465 字节，md5 `911476675ffbfb29a3144e6b53177a55`（实测可复现） |

同一条管线里的 **html2md**（C++ 侧，HTML → Markdown）是 1.7.0、MIT，已在 demo
以剪裁源码入库（`3rdparty/html2md/`，6 个文件 65K，源自
`aa-sources/html2md-1.7.0`）。

## 校验

三条路，按代价从低到高：

1. **对 md5**：与上表比。换了 esbuild 版本重建，字节可能变——那不算错，接着走 3。
2. **mermaid 专用**：`extract_mermaid_payload.py --check`（对 mermaidx 素材）。
3. **对行为**：`scripts/build_ninja.bat -t relwithdebinfo -a x86` 后跑 ctest，
   `tests/markdown` 那套把三个载荷的契约都钉住了（公式出 `<svg>`、图上文字与
   箭头、代码高亮的分词与转义）。**载荷是生成物、不可读也不可 review，行为判据
   是唯一的兜底**，换版本重建后必跑。
