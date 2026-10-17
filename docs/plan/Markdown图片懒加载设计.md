# Markdown 图片懒加载设计（阶段 2）

状态：讨论中，未实现。本文只覆盖主设计文档 `docs/plan/Markdown渲染库能力扩展设计.md`
§6.5 的**阶段 2**；阶段 1（上下文内图片字节 LRU）暂缓。

## 一、目标与范围

- 文档加载时不要一次性解码全部本地图片；
- 首屏图片立即可见，其余按事件循环分片加载；
- 加载过程中布局尺寸稳定，不出现图片到位后的整页跳动；
- 开启懒加载后，主题切换、渲染开关切换等重渲染路径仍然正确；
- 打印/导出 PDF 不受懒加载影响；
- 关闭开关时行为与现在逐字节一致。

不在本轮范围：网络图片（当前资源回调只支持 `data:` 与 base 目录内的本地文件）、
后台线程解码（先测量再决定）。

## 二、现状与约束

### 2.1 图片进入页面的时序

1. `QLiteHtmlWidget::setHtml()` → `DocumentContainer::setDocument()`；
   `setDocument()` 会清空 `m_pixmaps` 与动画表
   （`3rdparty/qlitehtml-180e3eb/src/container_qpainter.cpp:1161`）；
2. 解析/`compute_styles()` 阶段，`el_image::compute_styles()` 同步调用
   `container->load_image()`（`3rdparty/litehtml-0.10/src/el_image.cpp:56`）；
3. `load_image()` 调用宿主的 data callback，**立即**拿到全部字节并
   `QPixmap::loadFromData()`，结果存进 `m_pixmaps`
   （`container_qpainter.cpp:629-645`）；
4. 布局阶段 `el_image::get_content_size()` 调用
   `container->get_image_size()`，后者直接读 `m_pixmaps` 里的 pixmap 尺寸
   （`container_qpainter.cpp:771-790`）；
5. 绘制阶段 `draw_image()` 从 `m_pixmaps` 取图。

### 2.2 由此产生的硬约束

- `load_image` 在布局之前执行，此时拿不到元素坐标，**无法按视口判断
  “在不在首屏”**；
- 返回空字节会让 `m_pixmaps` 存一个 null pixmap，`get_image_size()` 得到
  0×0，布局塌陷；后续替换图片会导致整页跳动；
- `setDocument()` 清空 pixmap，所以切主题/改开关触发的重渲染会让所有图片
  重新走一遍加载；
- `QLiteHtmlWidget::print()` 另建一个 `DocumentContainer`，复用同一个 data
  callback，但不共享任何懒加载状态（`qlitehtmlwidget.cpp:577-616`）；
- litehtml 在“宽高都已知”的布局分支里不应用 `max-width:100%`
  （`3rdparty/litehtml-0.10/src/render_image.cpp`），因此不能靠给 `<img>`
  注入 `width`/`height` 属性来稳定布局。

## 三、占位方案比较

| 方案 | 做法 | 结论 |
|---|---|---|
| A. 全尺寸透明占位图 | 按 `QImageReader::size()` 生成真实尺寸的透明图 | 排除：1920×1080 ARGB 占位约 8MB，几十张图会打满内存 |
| B. 注入 width/height 属性 | 在 HTML 里补 `<img width height>`，布局不再依赖 pixmap | 排除：litehtml 在该分支忽略 `max-width`，宽图会溢出 |
| C. 尺寸查询回调 | 补丁新增 size handler；pixmap 无效时用它回答固有尺寸 | **采用**：不分配大占位图，保持现有 CSS/max-width 语义 |

## 四、qlitehtml 补丁 0004

### 4.1 公开 API

```cpp
using ImageSizeHandler = std::function<QSize(const QUrl&)>;
void QLiteHtmlWidget::setImageSizeHandler(const ImageSizeHandler& handler);

// 替换已加载（或占位）的图片。返回 false 表示数据无效。
// 新尺寸与布局所用尺寸不同才重排；相同则只重绘。
bool QLiteHtmlWidget::replaceImage(const QUrl& url, const QByteArray& data);
```

### 4.2 `DocumentContainer` 行为

- `get_image_size()`：
  1. pixmap 有效 → 用 pixmap 尺寸；
  2. pixmap 缺失/为空且 size handler 存在 → 调 handler，并把结果按文档
     缓存（同一次布局里会被反复询问，不能每次都回调）；
  3. handler 返回无效尺寸 → 保持 0×0（与当前行为一致）。
- `replaceImage()`：
  1. `QPixmap::loadFromData(data)` 失败或结果为空 → 返回 false，不动现有状态；
  2. 用新 pixmap 覆盖对应 URL 的条目；重新登记动画（先移除旧动画）；
  3. 新尺寸与布局尺寸比较：相同 → `viewport()->update()`；不同 →
     用 `withFixedTextPosition()` 包住 `render()`，保留滚动位置；
  4. 只在 GUI 线程调用（`QPixmap` 的线程约束）。
- `setDocument()` 继续清空 `m_pixmaps`/动画/每文档尺寸缓存；新文档重新建立。
- 补丁文件：`patches/qlitehtml/0004-replace-image.patch`，在
  `cmake/patch_dependencies.cmake` 注册；删除解压目录后重放验证。

## 五、库侧接入（`MarkdownView`）

### 5.1 开关

```cpp
void SetImageLazyLoadingEnabled(bool enabled);  // 默认 false
bool image_lazy_loading_enabled() const;
```

- 切换开关触发重渲染并保留滚动位置；
- 关闭时资源回调保持现有“一次性读全量”的行为。

### 5.2 资源回调三档

1. `data:` URI：直接返回字节（公式/Mermaid 的图本来就在文档里，不做延迟）；
2. 前 N 张本地图片：同步读全量，保证首屏；
3. 其余本地图片：
   - 用 `QImageReader` 只读文件头拿固有尺寸（`QImageReader::size()`），
     存入 `QHash<QUrl, QSize> image_sizes_`；
   - URL 进入 pending 队列，记录当前文档 generation；
   - 资源回调返回空字节；
   - 同一 URL 在一次文档内只入队一次（`load_image` 本身有 contains 检查，
     但重渲染后会重新回调，需要按 generation 去重）。

N 的取值建议：**张数 + 字节预算双限**，例如前 8 张且累计不超过 16MB；
一张 4K 图就占满预算时提前停止同步加载。

### 5.3 延迟加载调度

- `ApplyDocument()` 清空 pending 队列、递增 generation，然后
  `QTimer::singleShot(0, this, &MarkdownView::LoadPendingImages)`；
- 每个事件循环 tick 只处理一张图（或一个字节预算），读全量后调用
  `html_->replaceImage(url, data)`；
- 回调里校验 generation：旧文档的迟到任务直接丢弃；
- 队列空时停止定时器；
- 失败（读文件失败、格式不支持）标记该 URL 已完成，不再重试；该位置保持
  空白，与当前空字节行为一致。

### 5.4 尺寸与重渲染

- `image_sizes_` 按 URL 缓存，跨重渲染复用；
- 重渲染时 `setDocument()` 清空 pixmap，资源回调再次运行：已加载过的图片
  会重新入队（除非做字节缓存），尺寸仍然来自 `image_sizes_`，布局稳定；
- 可选：把阶段 1 的字节 LRU 作为本阶段配套。开启懒加载后，字节缓存能让
  切主题时不必重复读盘/解码；关闭懒加载时也可用。容量沿用 64MB。

### 5.5 打印/导出

- `Print()` 期间置 `force_full_images_`，资源回调强制全量返回；
- 打印完成后复位；打印容器不安装 size handler，也不需要 `replaceImage`；
- 需要一条测试：打印时资源回调的调用次数覆盖全部图片，且没有返回空字节。

## 六、边界情况

| 场景 | 处理 |
|---|---|
| 文件头读不出尺寸（损坏/不支持的格式） | 退回同步全量读，让 pixmap 自己给尺寸；仍失败则保持空 |
| 文件在“读头”和“读全量”之间被改 | `replaceImage` 检测尺寸变化并重排；可接受 |
| 动画 GIF | 占位阶段为空白；替换时重新登记 `QMovie`，动画继续可用 |
| SVG | `QImageReader::size()` 能读固有尺寸则走懒加载；读不出走同步回退 |
| CSS background-image | 同样经过 `load_image`/`get_image_size`，补丁对两者生效；当前 Markdown 输出不产生背景图 |
| 同一 URL 重复出现 | `m_pixmaps` 按 URL 去重；pending 按 URL 去重 |
| 文档在延迟加载途中被替换 | generation 校验丢弃旧任务 |
| 图片在 base_dir 之外 | 资源回调返回空（既有安全策略），不入队 |

## 七、测试计划

1. 关闭懒加载：现有图片测试逐字节不变；
2. 开启懒加载、图片数大于 N：前 N 张返回非空，其余为空且进入 pending；
3. 处理完事件循环后所有图片都已替换，布局坐标与全量加载时一致
   （用 `AnchorY` 比较后续标题位置）；
4. 打印路径：`force_full_images_` 生效，回调全部返回非空；
5. 切主题/重渲染后图片仍能到位（允许重新入队）；
6. 尺寸变化：替换一张固有尺寸与 hint 不同的图，触发重排且滚动位置保留；
7. 失败路径：损坏图片不崩溃，位置保持空白；
8. funq 抓图：md_viewer 打开含多图的文档，首屏可见、滚动后延迟图出现。

## 八、待确认决策

1. 补丁新增 `setImageSizeHandler` 是否接受（推荐方案 C 的前提）；
2. 是否把阶段 1 的图片字节 LRU 并入本阶段（推荐做，作为重渲染配套）；
3. 首屏 N 与字节预算的具体值（建议 8 张 / 16MB，双限）；
4. 解码是否保持在 GUI 线程按张分片（建议先保持；实测卡顿再评估后台
   `QImage` 解码）；
5. 是否在 `renderer_status()` 增加 deferred/replaced 计数，或复用现有
   `image_cache_hits/misses`。

## 九、实施顺序

1. 补丁 0004：size handler + replaceImage，重放验证；
2. `MarkdownView` 开关、资源回调分档、pending 调度、打印强制全量；
3. 单元测试与 funq 像素验收；
4. 可选：并入图片字节 LRU；
5. 更新主设计文档 §6.5 与使用文档。

## 十、风险

- 补丁面比 0003 大，后续跟上游合并要重新核对 `load_image`/`get_image_size`
  的实现变化；
- 布局稳定性依赖“读头尺寸等于真实解码尺寸”，个别格式可能不一致，需要
  尺寸变化重排作为兜底；
- 按张分片会把图片出现时间拉长；首屏 N 张太少会影响观感，太多会拖慢
  首次布局，需要实测调参；
- 打印路径是独立容器，任何状态没同步都会表现为 PDF 图片空白。
