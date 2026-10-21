# Markdown 查看器导出与图片交互设计

## 问题

md_viewer 暴露了 markdown 库目前缺少的四项能力：

1. 没有导出 HTML；现有只支持导出 PDF。
2. 搜索框清空后，文档里的查找选区仍然保留。
3. 图片没有右键菜单，不能另存为，也不能放大查看。
4. 点击公式/Mermaid 图片无法查看原始 TeX/Mermaid 源码。

前两项是宿主缺口，后两项是库缺口：qlitehtml 当前只暴露
`contextMenuRequested(pos, link_url)`，没有公开的“清空选区”和
“鼠标下是哪张图片”的入口。

## 现状与约束

- `MarkdownView` 已保存 `committed_html_` 和 `document_pixmaps_`。
- 公式/Mermaid 的 `<img src>` 使用 `mj:f/<hash>`、`mj:m/<hash>` token；
  token 对应的 QPixmap 在 `document_pixmaps_` 里。
- `MarkdownView` 目前没有保留 token → 原始公式/Mermaid 源码的映射。
- qlitehtml 的 `DocumentContainerPrivate::clearSelection()` 只存在于私有实现；
  `DocumentContainer` 和 `QLiteHtmlWidget` 都没有公开方法。
- qlitehtml 有 `linkAt()`，但没有 `imageAt()`；右键事件拿不到图片 URL。
- 导出 HTML 不能重新走同步渲染，否则会丢失当前异步渲染结果，也可能生成与
  屏幕不同的内容。

## 方案

### 1. 导出 HTML

库新增：

```cpp
bool ExportHtml(const QString& html_path, QString* error = nullptr) const;
```

导出结构：

```text
foo.html
foo_files/
  formula-<hash>.png
  mermaid-<hash>.png
  image-<hash>.<ext>
```

- 直接使用已提交的 `committed_html_`，保留当前主题 CSS。
- 把 `src="mj:..."` 替换为 `foo_files/...png`；公式/Mermaid 的 pixmap 从
  `document_pixmaps_` 取。任何 `mj:` 图片取不到或写盘失败，整次导出失败，
  不允许把 `mj:` 留在导出 HTML 里。
- 本地图片通过 `ReadResourceForUrl()` 读取，复制原始字节到资源目录。
  `src` 的解析顺序固定为：
  1. `UnescapeHtml()` 还原 `&amp;` 等实体；
  2. `QUrl::fromEncoded()` 解析百分号编码；
  3. 无 scheme 时以 `BaseUrlForDir(committed_base_dir_)` 为基准解析，
     不使用 `html_->url()`（它会剥掉最后一段路径）；
  4. 再交给 `ReadResourceForUrl()` 做目录边界校验。
- 写入 HTML 的资源相对路径先做百分号编码，再做 HTML 属性转义；资源目录名
  同样可能包含中文、空格、`&`、`#`，不能直接拼进属性。
- PNG data URI 解码后写盘；其它 `data:image/*` 先尝试 `QImage` 解码并转 PNG，
  失败则保留原 URI。普通本地图片读失败时保留原 `src`，并在 `error` 里报告
  警告；不回退 `mj:`。
- `http/https` 图片保持远程 URL；离线打开时仍依赖网络。
- 资源名按内容 SHA-256 去重；同名不同内容不会互相覆盖。
- 先创建资源目录并写完全部资源，最后用 `QSaveFile::commit()` 写 HTML；
  没有图片时不留下空目录。
- 本地图片语义是“导出时磁盘上的字节”，不是渲染瞬间的屏幕快照；文档里写明。
- md_viewer 文件菜单增加“导出 HTML…”，渲染中或没有文档时禁用/提示；
  `ExportHtml()` 自身也检查 `rendering_`。

### 2. 清空搜索选区

qlitehtml 增加一个很小的 vendor patch：

- `DocumentContainer::clearSelection()`：转调现有私有实现。
- `QLiteHtmlWidget::clearSelection()`：清选区并刷新 viewport。

库新增：

```cpp
void ClearSelection();
bool has_selection() const;  // !html_->selectedText().isEmpty()
```

md_viewer 在搜索框 `textChanged("")` 时调用；不改变现有查找命中行为。
`has_selection()` 给测试和宿主状态判断留一个稳定判据。

### 3. 图片右键与放大查看

qlitehtml 增加图片命中与信号：

```cpp
QUrl imageAt(const QPoint& viewport_pos) const;

signals:
    void imageClicked(const QUrl& url);
    void imageContextMenuRequested(const QPoint& global_pos, const QUrl& url);
```

- `imageAt(viewport_pos)` 的坐标契约明确为 viewport 坐标；内部走
  `viewport()->mapToParent()` + `htmlPos()` + `toVirtual()`，不直接用未缩放像素。
- `imageAt()` 用 `deepest_child_at_point()` 查 `img` 元素的 `src`。
- 左键必须在 press/release 命中同一张图、位移小于拖拽阈值、且沿祖先链
  没有 `<a>` 时才发 `imageClicked`；祖先链判断不能只看直接父级。
- 双击序列会在两次 release 各触发一次，因此加 300ms 同图去重，只打开一次。
- 右键命中图片时发 `imageContextMenuRequested(global_pos, url)`，同时保留
  原有 `contextMenuRequested` 给非图片区域。

`MarkdownView` 新增：

```cpp
QPixmap ImagePixmap(const QUrl& url) const;
```

支持 `mj:` token、`data:` 和文档目录内的本地图片；本地路径按
`committed_base_dir_` 解析，与资源回调一致。远程图片返回空 QPixmap，
md_viewer 的另存为/放大查看对空图禁用。资源图片的一次性读取不进入缓存，
避免放大查看把图片常驻内存。GIF 等动画图片另存为时直接复制原始字节，
不经过 QPixmap 重编码。

md_viewer 的右键菜单：

- 另存为…
- 放大查看
- 查看源码（仅公式/Mermaid）

放大查看使用 `QGraphicsView` + `QGraphicsPixmapItem`，滚轮缩放、拖拽平移、
双击在“适应窗口/100%”之间切换；不引入第三方依赖。

### 4. 公式和 Mermaid 查看源码

库新增公开结构：

```cpp
struct MarkdownImageInfo {
  MarkdownRenderKind kind = MarkdownRenderKind::Highlight;
  QString source;
  bool display = false;
};

std::optional<MarkdownImageInfo> ImageInfo(const QUrl& url) const;
```

- `CommitRender` 的 resolver 用局部 `QHash` 收集映射，和 `pixmaps` 一起在
  提交段 move；只记录返回 tag 里真的含 token 的图片。`TokenizeImageTag`
  失败退回 data URI 时不记录，`ImageInfo` 返回 `nullopt`，按普通图片处理。
- 公式存 `payload.trimmed()`，Mermaid 存原始代码；同一 token 多次出现时
  后写覆盖前写。key 用与 pixmap provider 相同的
  `QString::fromLatin1(url.toEncoded())`。
- `ShowMessage()` 清空映射；`CancelPendingRender()`/`ClearDocument()` 保留，
  因为屏幕上的旧文档还在。
- `ImageInfo` 对普通图片返回 `nullopt`。
- md_viewer 左键点击公式/Mermaid 图片时打开源码对话框；普通图片打开预览。
- 源码对话框只读，使用等宽字体；公式带 `$...$`/`$$...$$` 分隔符，提供复制。

## 公开 API 汇总

```cpp
// markdown_view.h
void ClearSelection();
bool has_selection() const;
bool ExportHtml(const QString& html_path, QString* error = nullptr) const;
QPixmap ImagePixmap(const QUrl& url) const;
std::optional<MarkdownImageInfo> ImageInfo(const QUrl& url) const;

signals:
  void imageClicked(const QUrl& url);
  void imageContextMenuRequested(const QPoint& global_pos, const QUrl& url);
```

## 测试与验证

- `ExportHtml` 单元测试：中文/空格/`&`/`#` 路径、公式与 Mermaid token 替换、
  本地图片复制、PNG data URI、同名内容去重、读失败保留原 src、远程 URL 保留、
  无图片不建目录。
- `ClearSelection` 单元测试：`FindText` → `has_selection()` 为 true →
  `ClearSelection()` → false → 再 `FindText` 仍能命中。
- 图片交互单元测试（Widgets + QTest，不走 funq 物理点击）：
  - `imageAt()` 命中/未命中；
  - 直接向 `QLiteHtmlWidget` 发送 `QMouseEvent` 验证 `imageClicked`；
  - 直接发送 `QContextMenuEvent` 验证 `imageContextMenuRequested`；
  - 链接内图片不触发 `imageClicked`；
  - `ImageInfo` 对公式、重复公式、缓存命中、切主题、普通图片分别断言。
  说明：QTest 的 `mouseClick` 在 `QAbstractScrollArea` viewport 上不经过 qlitehtml
  的鼠标处理，测试必须直接打控件入口。
- `MarkdownView::ImageInfo` 的 token 可由公开的 `MakeImageToken()` +
  `FormulaRenderKey()` 构造；测试目标显式 include/link qlitehtml 后才能
  找到内部控件。
- md_viewer 的新菜单动作、预览窗口、源码对话框都设 objectName；funq 只做
  “动作存在/可见”断言，不依赖坐标点击或原生保存对话框。
- 现有 `ctest -L MARKDOWN` 全量回归；32 位 relwithdebinfo 编译通过。

## 风险与取舍

- qlitehtml 补丁必须落在 `patches/qlitehtml/0009-image-hit-and-selection.patch`，
  由 `cmake/patch_dependencies.cmake` 注册，并在 configure 日志里确认 `[OK]`；
  不直接修改解压目录。补丁针对“已打 0001–0008”的树生成。
- HTML 导出按资源目录方案，分享时必须连目录一起带走；这是用户已确认的选择。
- 远程图片不下载，只保留 URL；本地图片以导出时磁盘字节为准。
- 点击图片与文本选区的边界用拖拽阈值、同图判断和 300ms 去重隔离。
- 源码映射只对已提交文档有效；渲染中的占位页没有可查看图片。
- 导出的 HTML 不会重写指向其它 `.md` 的相对链接，分享后这些链接可能失效。

## 实施顺序

1. 新增并应用 qlitehtml `0009` patch：清选区 + 图片命中/信号，确认 configure
   日志出现 `[OK]`。
2. 补 `MarkdownView` 导出、选区、图片查询和源码映射。
3. md_viewer 增加导出菜单、右键菜单、预览和源码对话框。
4. 补无头单元测试与 objectName 断言，跑 MARKDOWN 分组回归和 x86 编译。
