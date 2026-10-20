# Markdown 公式/图表 QPixmap 直供设计

状态：已实施（2026-10-20）。qlitehtml 补丁 0008 已落地并重放验证；
markdown 侧 token 直供、缓存与测试已通过。0007 由
`docs/plan/Markdown图片懒加载设计.md` 预留。

## 一、qlitehtml 补丁

1. `src/container_qpainter.h` 加接口

   ```c++
   #include <QPixmap>

   using PixmapCallBack = std::function<QPixmap(const QUrl &)>;
   void setPixmapCallBack(const PixmapCallBack &callback);
   PixmapCallBack pixmapCallBack() const;
   ```

2. `src/container_qpainter_p.h` 加成员

   ```c++
   DocumentContainer::PixmapCallBack pixmap_callback_;
   ```

3. `src/container_qpainter.cpp` 实现存取并改 `load_image`

   ```c++
   void DocumentContainer::setPixmapCallBack(const PixmapCallBack &callback)
   {
       d->pixmap_callback_ = callback;
   }

   DocumentContainer::PixmapCallBack DocumentContainer::pixmapCallBack() const
   {
       return d->pixmap_callback_;
   }

   void DocumentContainerPrivate::load_image(const char *src, const char *baseurl, bool redraw_on_ready)
   {
       const auto qtSrc = QString::fromUtf8(src);
       const auto qtBaseUrl = QString::fromUtf8(baseurl);
       Q_UNUSED(redraw_on_ready)
       qDebug(log) << "load_image:" << QString("src = \"%1\";").arg(qtSrc).toUtf8().constData()
                   << QString("base = \"%1\"").arg(qtBaseUrl).toUtf8().constData();
       const QUrl url = resolveUrl(qtSrc, qtBaseUrl);
       if (m_pixmaps.contains(url))
           return;

       // 先查外部 provider
       if (pixmap_callback_) {
           const QPixmap pixmap = pixmap_callback_(url);
           if (!pixmap.isNull()) {
               m_pixmaps.insert(url, pixmap);
               return;  // 静态图，跳过 loadAnimation
           }
       }
       const QByteArray data = m_dataCallback(url);
       QPixmap pixmap;
       pixmap.loadFromData(data);
       m_pixmaps.insert(url, pixmap);
       loadAnimation(url, data);
   }
   ```

   provider 返回空 QPixmap 落回 DataCallback；`mj:` 之外的 URL（本地文件、真正的
   data URI）行为不变。

4. `src/qlitehtmlwidget.h/.cpp` 加公开入口

   ```c++
   // qlitehtmlwidget.h
   #include <QPixmap>

   using PixmapCallBack = std::function<QPixmap(const QUrl &)>;
   void setPixmapCallBack(const PixmapCallBack &callback);
   ```

   ```cpp
   // qlitehtmlwidget.cpp
   void QLiteHtmlWidget::setPixmapCallBack(const PixmapCallBack &callback)
   {
       d->documentContainer.setPixmapCallBack(callback);
   }
   ```

   > [!important]
   > 必须在第一次 `setHtml()` 之前设置。litehtml 在
   > `document::createFromString()` 的 `compute_styles()` 阶段就会对每个 `<img>`
   > 调 `load_image`，不是等到 `render()`。

5. 必须同步改 `QLiteHtmlWidget::print`

   ```cpp
   void QLiteHtmlWidget::print(QPrinter *printer)
   {
   #if QLITEHTML_HAS_QPRINTER
       QPainter painter;
       if (!painter.begin(printer))
           return;

       DocumentContainer dc;
       dc.setDataCallback(d->documentContainer.dataCallback());
       dc.setPaletteCallback(d->documentContainer.paletteCallback());
       dc.setPixmapCallBack(d->documentContainer.pixmapCallBack());  // 补上
       dc.setDefaultFont(d->documentContainer.defaultFont());
       dc.setPaintDevice(printer);
       dc.setBaseUrl(d->documentContainer.baseUrl());
       dc.setMediaType(DocumentContainer::MediaType::Print);
       dc.setDocument(d->html.toUtf8(), &d->context);

       // .............
   }
   ```

   `setPixmapCallBack` 在 `setDocument` 之前；否则导出的 PDF 没有图。

6. `setDocument` 保留 `m_pixmaps.clear()`

   跨文档复用 pixmap 会破坏两条语义：

   - 动画图：`m_animations` 每次都会清，pixmap 命中后 `loadAnimation` 不再执行，
     GIF 退化成静态图。
   - 本地图片：watchdog 重载后图片内容变了、URL 没变，旧 pixmap 会一直命中。

   图片缓存由宿主承担，qlitehtml 只维护文档级生命周期。

7. provider miss 日志放宿主

   qlitehtml 内部不对 provider 返回空告警：本地图片和真正的 data URI 走回退是正常
   路径。宿主只对 `mj:` miss 打 `qWarning`。

## 二、补丁落地

1. 补丁文件：`patches/qlitehtml/0008-pixmap-callback.patch`。

   > [!important]
   > 0007 已被 `docs/plan/Markdown图片懒加载设计.md` 预留为
   > `0007-replace-image.patch`。若 0007 先落地，它和本补丁都会改
   > `qlitehtmlwidget.h/.cpp` 与 `load_image`，0008 必须基于含 0007 的树重出 diff。

2. 生成：在已应用 0001–0006 的 `3rdparty/qlitehtml-180e3eb` 上先做快照，再改
   `container_qpainter.h`、`container_qpainter_p.h`、`container_qpainter.cpp`、
   `qlitehtmlwidget.h`、`qlitehtmlwidget.cpp` 五个文件，用快照对当前生成 unified
   diff，路径保持 `a/src/...` / `b/src/...`。

3. 校验与注册：

   - 手动 `git apply --check` 按 helper 的环境来：在被补丁目录的父目录设
     `GIT_CEILING_DIRECTORIES`，并加 `-c core.autocrlf=false`；否则 ignored
     untracked 文件可能被静默跳过且返回 0。
   - 补丁必须 LF（`.gitattributes` 的 `patches/** -text`）。
   - 在 `cmake/patch_dependencies.cmake` 的 qlitehtml 段最后注册：

     ```cmake
     apply_patch_to_dir("${3RDPARTY_DIR}/qlitehtml-180e3eb"
         "${PATCHES_DIR}/qlitehtml/0008-pixmap-callback.patch")
     ```

   - 删除 marker 不会还原手工改过的文件。重放验证前从快照恢复 pristine 树（或
     重新解压目录），再 configure 确认 `[OK]`，然后做一次干净编译。
   - 若 0007 先落地，cmake 里 0007 必须注册在 0008 之前。
   - 本次只改既有文件，`src/CMakeLists.txt` 的源文件列表不用动。

## 三、markdown 侧配套

1. token 格式

   公式：`mj:f/<sha256>`；mermaid：`mj:m/<sha256>`。

   - hash 用 `QCryptographicHash::Sha256`，与 `render_cache.cpp` 一致。
   - token 由渲染器的内容缓存 key 派生，不能按 ApplyDocument 递增：渲染器内存/
     磁盘缓存命中时直接返回旧 tag，不会重新产出 QImage。
   - key 统一用 `url.toEncoded()` 或同一个 helper 生成，避免 `toString()` 选项
     差异导致查不到。

   > [!important]
   > 写成 `mj:f/<hex>`，不要 `mj://f/<hex>`。`resolveUrl` 对非空 scheme 原样
   > 返回；`mj://0` 会被 QUrl 当 host 规范化。`mj:f/<hex>` 没有 authority。

2. 渲染器接口加 `cache_key` 出参

   ```c++
   QString FormulaToImgTag(MarkdownRenderContext &context, const QString &tex,
                           bool display, const QColor &color,
                           QString *cache_key = nullptr);
   QString MermaidToImgTag(MarkdownRenderContext &context, const QString &code,
                           MarkdownTheme theme, QString *cache_key = nullptr);
   ```

   - 在 key 计算完成后立刻 `if (cache_key) *cache_key = key;`，放在任何 return
     之前。这样内存命中、磁盘命中、正常渲染、引擎加载失败四条路径都会填。
   - 失败路径返回空 tag 时 key 仍然有值；宿主在 `renderer_tag.isEmpty()` 时直接
     返回空，不生成 token，也不入缓存。
   - 直接修改现有声明（公式 4 参加第 5 参、mermaid 3 参加第 4 参，末尾默认
     `nullptr`），不要新增带默认参数的重载，否则现有调用会二义。

3. 两层缓存

   - 跨 ApplyDocument 层放进 `MarkdownRenderContextState`
     （`markdown_render_context_internal.h`）：

     ```c++
     struct PixmapCacheEntry {
         QPixmap pixmap;
         QString img_tag;  // 已替换成 mj: token 的 <img ...>
     };

     // key 为 token 字符串 mj:f/... 或 mj:m/...
     QCache<QString, PixmapCacheEntry> pixmap_cache;
     ```

     - 构造函数和 `SetOptions()` 同步 maxCost：`image_cache_bytes` 是 qint64，
       QCache 的 cost 是 int；按模块惯例 0/负值表示无上限，用 `INT_MAX`，
       大于 `INT_MAX` 时截断。
     - `SetOptions()` 调 `Trim()`；`ClearMemoryCaches()` 清 `pixmap_cache`。
     - `MarkdownView` 通过 `markdown_render_context_internal.h` 访问该层；
       命中/未命中写入 `state.image_stats.image_cache_hits/misses`。

   - 当前文档层放在 `MarkdownView`：

     ```c++
     // 当前文档 pin 住；ApplyDocument 开头清空
     QHash<QString, QPixmap> document_pixmaps_;
     ```

   - `ApplyDocument()` 开头 `document_pixmaps_.clear()`；sink 在生成 HTML 时逐张
     注册，所以进入 `setHtml()` 时当前文档的所有 token 都已 pin 住。
   - `RegisterImageTag(token, renderer_tag)`：

     1. `renderer_tag` 为空 → 返回空，走原文兜底，不入缓存。
     2. `pixmap_cache` 命中 → 复制 QPixmap 进 `document_pixmaps_`，
        返回 `entry->img_tag`。
     3. 未命中：从 `renderer_tag` 取 `src`，按第 6 条的上限解码一次成 QPixmap。
     4. 解码失败 → 返回原始 `renderer_tag`，不入缓存。
     5. 成功：只替换 `src="..."` 的属性值，`class` / `width` / `height` 与属性
        顺序逐字节保留；生成 token tag。
     6. `pixmap_cache.insert(token, entry, cost)`，cost 用 int，超过 `INT_MAX`
        时截断。Qt5 的 `QCache::insert` 在 `cost > maxCost` 时删除对象并返回
        false，失败后不能再用 `entry` 指针；无论成败，都用本地 `QPixmap` 副本
        pin 进 `document_pixmaps_`，再返回 token tag。

   > [!important]
   > provider 不能直接用跨 ApplyDocument QCache 返回图片：当前文档超过 64MB 或单张图超过
   > 预算时，QCache 会淘汰本文件正在用的条目，token 立即悬空、图片空白且本次
   > 文档内不会重试。`document_pixmaps_` 这一层保证当前文档的 token 一定可解析。

4. provider 闭包

   ```cpp
   html_->setPixmapCallBack([this](const QUrl &url) {
       if (url.scheme() != QLatin1String("mj")) {
           return QPixmap();  // 本地文件/真 data URI 走原 DataCallback
       }
       const QString token = QString::fromLatin1(url.toEncoded());
       const auto it = document_pixmaps_.constFind(token);
       if (it == document_pixmaps_.constEnd()) {
           qWarning() << "markdown: pixmap provider miss for" << url;
           return QPixmap();
       }
       return *it;
   });
   ```

   key 与 `RegisterImageTag` 用同一个 token 字符串；provider 只读
   `document_pixmaps_`，不查 QCache。

5. DataCallback 对 `mj:` 不报失败

   provider miss 后 qlitehtml 会回退 DataCallback；`ReadResourceForUrl` 对
   `mj:` 返回空，现有代码会 `emit resourceLoadFailed(url)`。在
   `setResourceHandler` 的 lambda 里对 `mj:` 提前返回空 QByteArray，不打该信号；
   需要时只写 trace。`markdown_view.h` 中 `resourceLoadFailed` 的注释同步改成
   “data: URI 与 mj: token 不计入”。

6. 解码上限

   `RegisterImageTag` 处理的是渲染器自己产出的 data URI，不走
   `ReadResourceForUrl` 的 16MB 防恶意输入上限，但按像素面积设独立预算：

   - 解码前按 base64 编码长度预检，解码后的 PNG 字节 ≤ 64MiB。
   - 用 `QImageReader` 先读尺寸，要求 `qint64(w) * h * 4 <= 64MiB`，并加单边
     硬上界 32767（QImage 安全值）。不按单边 4096 判断：mermaid 允许 4096²
     面积内的长图（如 12949×1295）。
   - 给 `formula_renderer.cpp` 的 `SvgToImgTag` 补上与 mermaid 相同的面积封顶，
     保证渲染器不会先分配超限大图。
   - 文档作者手写的 data URI 仍走 `ReadResourceForUrl` 原有的 16MB 压缩字节
     限制。渲染器产出的图注册失败会退回原 data URI，这时才受该限制；超限打
     `qWarning` 并按原文兜底。

7. 缓存配置与生命周期

   - 容量复用 `MarkdownRenderContextOptions::image_cache_bytes`（默认 64MB），
     在 `MarkdownRenderContext` 的构造和 `SetOptions()` 同步到
     `pixmap_cache.maxCost`。
   - `ClearRenderCaches()` → `context_->ClearMemoryCaches()`：清公式/Mermaid 内存
     缓存与跨 ApplyDocument `pixmap_cache`，**不清 `document_pixmaps_`**。后者只在
     `ApplyDocument()` 开头清，保证当前文档已生成的 token 在 PDF 导出等场景仍
     可解析。
   - `markdown_view.h` 里 `ClearRenderCaches()` 的注释同步改成“清公式/Mermaid
     与跨 ApplyDocument 图片缓存，当前文档 pin 保留”。
   - 命中/未命中计入 `image_context` 的 `image_cache_hits` / `image_cache_misses`，
     用于验证“重复 setHtml 不再解码”。

8. sink 接线

   ```cpp
   formula_sink = [this](const QString &tex, bool display) {
       QString key;
       const QString tag = FormulaToImgTag(*context_, tex, display,
                                           ThemeFormulaColor(theme_), &key);
       return RegisterImageTag(TokenFor(QStringLiteral("f/"), key), tag);
   };
   mermaid_sink = [this](const QString &code) {
       QString key;
       const QString tag = MermaidToImgTag(*context_, code, theme_, &key);
       return RegisterImageTag(TokenFor(QStringLiteral("m/"), key), tag);
   };
   ```

   `TokenFor()` 返回 `"mj:" + prefix + Sha256Hex(key)`；`Sha256Hex` 用
   `QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex()`。
   需要 `#include <QCryptographicHash>`、`<QCache>`、`<QPixmap>`。

## 四、约束与已知限制

- QImage → QPixmap、注册 pixmap 都在 GUI 线程。
- provider 在 `setDocument` / `render` / `print` 内同步调用，不能重入
  `setHtml()` / `ApplyDocument()`；将来做 miss 恢复必须走
  `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`，不能在 `load_image`
  内重排。
- 若将来把 `MarkdownToHtml` 搬进 worker 线程，sink 只能产出 QImage/token；
  QPixmap 的创建与注册必须回 GUI 线程。
- 不给 pixmap 设 `devicePixelRatio`；qlitehtml 按显式目标矩形绘制，2x 像素 +
  逻辑 `width/height` 即可。
- provider 只处理 `mj:`，公式/图表是静态图；本地 GIF 继续走 DataCallback。
- 本次收益只覆盖公式/图表：本地图片仍会在每次 `setHtml` 重新读盘、解码。
- `document_pixmaps_` 不受 `image_cache_bytes` 约束，只随当前文档内容增长。
- 单张 4096² 图的 cost 约 64MiB，加上 img_tag 后超过默认 `image_cache_bytes`
  的 maxCost，进不了跨 ApplyDocument QCache；pin 层仍保证显示，但每次 `ApplyDocument`
  会重新解码一次。这是已知边界，不阻塞本次实施。
- 打印容器按 `resolution()/96` 放大绘制，高 dpi PDF 中 2x 位图会被插值放大；
  这是已知限制，不阻塞本次实施。

## 五、测试

- 连续两次 `ApplyDocument` 后图片仍在。
- 第二次 `ApplyDocument` 不增加渲染器 `renders` 计数，`image_context` 命中数增加。
- 切主题、切行号后公式/图表仍在。
- 进程重启后读磁盘缓存，首次 `ApplyDocument` 图片能出来（解码一次后命中）。
- 设置较小的 `image_cache_bytes`，先断言 `QCache::maxCost()` 已同步；再多图超过
  预算，当前文档图片仍全部显示（验证 pin 层），跨 ApplyDocument 缓存淘汰不产生悬空 token。
- 单张 4096×4096 图能显示（pin 层），并接受它进不了跨 ApplyDocument QCache、每次重解码。
- `ClearRenderCaches()` 后屏幕与 PDF 图片仍在（验证 pin 层不被清），随后再
  `ApplyDocument` 才重建。
- renderer 返回空串时 sink 返回空，公式/代码块走原文兜底。
- data URI 损坏时返回原始 tag 且不入缓存。
- 本地图片、动画 GIF 行为与改动前一致。
- PDF 导出含图（`print()` 复制 provider）。
- 更新 `tests/markdown/render_toggles_test.cpp:82`：启用公式后断言 HTML 含
  `mj:f/`，不再断言 `data:image/png;base64`；禁用断言保持不变。
- 把 `TokenFor` / `RegisterImageTag` 的解码与替换核心抽成自由函数（与
  `ReadResourceForUrl` 同理），直接测空 tag、损坏 data URI、`class` / `width` /
  `height` 属性逐字节保留、`QCache::insert` 失败。
- `QUrl(QStringLiteral("mj:f/<64hex>")) == QUrl::fromEncoded(...)`，
  `host()` 为空，`toString()` 与 `toEncoded()` 都等于 token。
- `mj:` provider miss 打 `qWarning`，且不触发 `resourceLoadFailed`。
