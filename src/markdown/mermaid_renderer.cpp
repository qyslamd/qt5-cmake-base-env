#include "mermaid_renderer.h"

#include <quickjs.h>

#include <cmath>

#include <QBuffer>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRegularExpression>
#include <QStringList>
#include <QSvgRenderer>

#include "mermaid_css_inliner.h"
#include "mermaid_svg_cleanup.h"

// 静态库里的 qrc 不会自动初始化，而载荷就在里面。Q_INIT_RESOURCE 展开的是
// extern 声明 + 一次调用，**必须留在文件作用域、且不能放进匿名命名空间**
// （放进去会绑到匿名空间上，链接期找不到 rcc 生成的全局函数）。重复调用幂等
static void EnsureResourcesInitialized() { Q_INIT_RESOURCE(markdown_res); }

namespace {

JSRuntime* g_runtime = nullptr;
JSContext* g_context = nullptr;
bool g_load_failed = false;
// key = mermaid 源码。失败也缓存空串：同一个坏图不该每次编辑都重试。
// 上限 32 张（每张是内嵌 PNG 的 data URI，大图能到 MB 级），FIFO 淘汰——
// 被挤掉的图无非重渲一次，无限增长的缓存会在长会话里把内存吃穿
constexpr int kCacheMax = 32;
QHash<QString, QString> g_cache;
QList<QString> g_cache_order;

void CacheInsert(const QString& key, const QString& tag) {
  if (!g_cache.contains(key)) {
    g_cache_order.append(key);
  }
  g_cache.insert(key, tag);
  while (g_cache_order.size() > kCacheMax) {
    g_cache.remove(g_cache_order.takeFirst());
  }
}

// 排查渲染问题用：设了 MD_MERMAID_DUMP=<目录> 就把各阶段产物落盘
QString DumpDir() {
  static const QString dir = qEnvironmentVariable("MD_MERMAID_DUMP");
  return dir;
}

void DumpStage(int index, const QString& stage, const QByteArray& data) {
  const QString dir = DumpDir();
  if (dir.isEmpty()) {
    return;
  }
  QFile file(dir + QStringLiteral("/dbg_%1_%2").arg(index).arg(stage));
  if (file.open(QIODevice::WriteOnly)) {
    file.write(data);
  }
}

// ---- 字体 ----
//
// 度量与绘制必须是同一份字体，否则文字会偏移或溢出。做法是选一个平台上有
// CJK 的族名，度量用它回答 mermaid，绘制前再把 SVG 里的 font-family 强制成
// 它（见 InlineSvgCss 的第三个参数）。找不到就返回空串：此时不改写，度量按
// SVG 自己写的族名走——两者都过 Qt 的替换链，仍然同源，只是中文可能缺字。

QString PickCjkFamily() {
  // 只许在 GUI 线程、QGuiApplication 构造之后调：QFontDatabase 需要应用实例。
  // 用运行时检查不用 Q_ASSERT——Release 下断言是空操作，null 会直接炸在
  // families() 里；返回空族，调用方退回 JS 自报的族名，行为可预期
  if (qGuiApp == nullptr) {
    qWarning() << "mermaid: PickCjkFamily called before QGuiApplication";
    return QString();
  }
  const QStringList candidates = {
      QStringLiteral("Microsoft YaHei"),     // Windows
      QStringLiteral("Noto Sans CJK SC"),    // Linux 常见
      QStringLiteral("Source Han Sans SC"),  // 思源黑体
      QStringLiteral("WenQuanYi Micro Hei"), QStringLiteral("SimHei"),
  };
  // families() 在 Qt 5.x 全程都是成员函数（Qt 6 才改成静态），得先建实例
  const QFontDatabase database;
  const QStringList families = database.families();
  for (const QString& name : candidates) {
    if (families.contains(name)) {
      return name;
    }
  }
  qWarning() << "mermaid: no CJK font family found, diagram text may show"
                " as boxes";
  return QString();
}

bool IsBoldWeight(const QString& weight) {
  bool ok = false;
  const int numeric = weight.toInt(&ok);
  if (ok) {
    return numeric >= 600;
  }
  const QString lower = weight.trimmed().toLower();
  return lower == QLatin1String("bold") || lower == QLatin1String("bolder");
}

// mermaid 的度量回调：QFontMetricsF 走的是 QFont 的替换链，与 QSvgRenderer
// 绘制时同一条路。不用 QRawFont——它的 advancesForGlyphs 不做字体替换，
// 缺字时会与绘制对不上。
// 前置条件同 PickCjkFamily：GUI 线程 + QGuiApplication 已构造
QFontMetricsF MetricsFor(const QString& js_family, double size_px,
                         const QString& weight) {
  const QString family = MermaidFontFamily();
  QFont font(family.isEmpty() ? js_family : family);
  font.setPixelSize(qMax(1, qRound(size_px)));
  font.setBold(IsBoldWeight(weight));
  return QFontMetricsF(font);
}

struct TextMeasure {
  double width = 0.0;
  double ascent = 0.0;
  double descent = 0.0;  // 正数
};

TextMeasure MeasureText(const QString& text, double size_px,
                        const QString& js_family, const QString& weight) {
  TextMeasure out;
  if (text.isEmpty() || size_px <= 0.0) {
    return out;
  }
  const QFontMetricsF metrics = MetricsFor(js_family, size_px, weight);
  out.width = metrics.horizontalAdvance(text);
  out.ascent = metrics.ascent();
  out.descent = metrics.descent();
  return out;
}

// ---- QuickJS ----

JSValue JsLogRaw(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc >= 1) {
    const char* text = JS_ToCString(ctx, argv[0]);
    if (text != nullptr) {
      qDebug().noquote() << "[mermaid/js]" << text;
      JS_FreeCString(ctx, text);
    }
  }
  return JS_UNDEFINED;
}

JSValue JsMeasureTextRaw(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 5) {
    return JS_NewFloat64(ctx, 0.0);
  }
  const char* text = JS_ToCString(ctx, argv[0]);
  double size = 16.0;
  JS_ToFloat64(ctx, &size, argv[1]);
  const char* family = JS_ToCString(ctx, argv[3]);
  const char* weight = JS_ToCString(ctx, argv[4]);
  const TextMeasure measure =
      MeasureText(QString::fromUtf8(text != nullptr ? text : ""), size,
                  QString::fromUtf8(family != nullptr ? family : ""),
                  QString::fromUtf8(weight != nullptr ? weight : ""));
  if (text != nullptr) {
    JS_FreeCString(ctx, text);
  }
  if (family != nullptr) {
    JS_FreeCString(ctx, family);
  }
  if (weight != nullptr) {
    JS_FreeCString(ctx, weight);
  }
  return JS_NewFloat64(ctx, measure.width);
}

JSValue JsMeasureTextFullRaw(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  static const char* const kEmpty = "{\"width\":0,\"ascent\":0,\"descent\":0}";
  if (argc < 5) {
    return JS_NewString(ctx, kEmpty);
  }
  const char* text = JS_ToCString(ctx, argv[0]);
  double size = 16.0;
  JS_ToFloat64(ctx, &size, argv[1]);
  const char* family = JS_ToCString(ctx, argv[3]);
  const char* weight = JS_ToCString(ctx, argv[4]);
  const TextMeasure measure =
      MeasureText(QString::fromUtf8(text != nullptr ? text : ""), size,
                  QString::fromUtf8(family != nullptr ? family : ""),
                  QString::fromUtf8(weight != nullptr ? weight : ""));
  if (text != nullptr) {
    JS_FreeCString(ctx, text);
  }
  if (family != nullptr) {
    JS_FreeCString(ctx, family);
  }
  if (weight != nullptr) {
    JS_FreeCString(ctx, weight);
  }

  QJsonObject object;
  object[QStringLiteral("width")] = measure.width;
  object[QStringLiteral("ascent")] = measure.ascent;
  object[QStringLiteral("descent")] = measure.descent;
  const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
  return JS_NewStringLen(ctx, json.constData(), json.size());
}

bool EvalGlobal(const QByteArray& code, const char* name) {
  const JSValue value = JS_Eval(g_context, code.constData(), code.size(), name,
                                JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(value)) {
    JSValue exception = JS_GetException(g_context);
    const char* message = JS_ToCString(g_context, exception);
    qWarning() << "mermaid: eval" << name
               << "failed:" << (message != nullptr ? message : "?");
    if (message != nullptr) {
      JS_FreeCString(g_context, message);
    }
    JS_FreeValue(g_context, exception);
    JS_FreeValue(g_context, value);
    return false;
  }
  JS_FreeValue(g_context, value);
  return true;
}

bool ReadPayload(const QString& path, QByteArray* out) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "mermaid: payload missing:" << path;
    return false;
  }
  *out = file.readAll();
  return true;
}

bool EnsureLoaded() {
  if (g_context != nullptr) {
    return true;
  }
  if (g_load_failed) {
    return false;
  }

  EnsureResourcesInitialized();
  QByteArray path_bbox;
  QByteArray dom_shim;
  QByteArray mermaid_js;
  if (!ReadPayload(QStringLiteral(":/markdown/path_bbox.js"), &path_bbox) ||
      !ReadPayload(QStringLiteral(":/markdown/dom_shim.js"), &dom_shim) ||
      !ReadPayload(QStringLiteral(":/markdown/mermaid.js"), &mermaid_js)) {
    g_load_failed = true;
    return false;
  }

  g_runtime = JS_NewRuntime();
  if (g_runtime == nullptr) {
    g_load_failed = true;
    return false;
  }
  // mermaid 布局递归深，默认配额不够。16MB 与 formula_renderer 一致，且必须
  // ≤ 宿主线程栈（配方见 src/markdown/CMakeLists.txt 的 /STACK）——JS 配额是
  // 软上限（超了抛可捕获异常），线程栈是硬的（超了直接崩）
  JS_SetMaxStackSize(g_runtime, 16U << 20);
  g_context = JS_NewContext(g_runtime);
  if (g_context == nullptr) {
    g_load_failed = true;
    JS_FreeRuntime(g_runtime);
    g_runtime = nullptr;
    return false;
  }

  JSValue global = JS_GetGlobalObject(g_context);
  JS_SetPropertyStr(g_context, global, "__log_raw",
                    JS_NewCFunction(g_context, JsLogRaw, "__log_raw", 1));
  JS_SetPropertyStr(
      g_context, global, "__measureText_raw",
      JS_NewCFunction(g_context, JsMeasureTextRaw, "__measureText_raw", 5));
  JS_SetPropertyStr(g_context, global, "__measureTextFull_raw",
                    JS_NewCFunction(g_context, JsMeasureTextFullRaw,
                                    "__measureTextFull_raw", 5));
  JS_FreeValue(g_context, global);

  // dom_shim 期望的是"包过一层"的函数名；__measureTextFull 的契约是对象，
  // 而 C 侧只能返回字符串，所以在这里 JSON.parse
  static const char* const kBridge =
      "globalThis.__log = (s) => __log_raw(s);\n"
      "globalThis.__measureText = (t,s,f,w,st) => "
      "__measureText_raw(t,s,f,w,st);\n"
      "globalThis.__measureTextFull = (t,s,f,w,st) => "
      "JSON.parse(__measureTextFull_raw(t,s,f,w,st));\n";
  if (!EvalGlobal(QByteArray(kBridge), "bridge")) {
    g_load_failed = true;
    return false;
  }

  // 顺序固定：path_bbox（纯几何）→ dom_shim（假 DOM）→ mermaid（3.5MB bundle）
  if (!EvalGlobal(path_bbox, "path_bbox.js") ||
      !EvalGlobal(dom_shim, "dom_shim.js") ||
      !EvalGlobal(mermaid_js, "mermaid.js")) {
    g_load_failed = true;
    return false;
  }

  // mermaid.js 是 esbuild 打的 ESM，手动把 default 暴露出来
  static const char* const kExpose =
      "globalThis.mermaid = "
      "(globalThis.__esbuild_esm_mermaid_nm.mermaid.default"
      " || globalThis.__esbuild_esm_mermaid_nm.mermaid);";
  if (!EvalGlobal(QByteArray(kExpose), "expose")) {
    g_load_failed = true;
    return false;
  }
  return true;
}

// 渲染：mermaid.render() 返回 Promise，而 QuickJS 的微任务不会自动跑，必须
// 手动泵（dom_shim 把 rAF/setTimeout 换成了微任务，所以循环能收敛）
QString RenderMermaidSvg(const QString& code) {
  if (!EnsureLoaded()) {
    return QString();
  }
  if (!EvalGlobal(QByteArray("__resetDocument();"), "reset")) {
    return QString();
  }

  // htmlLabels:false 是必填项：默认会把节点文字放进 <foreignObject>，而
  // QSvgRenderer 完全不认，文字会全部消失
  static const char* const kInit =
      "mermaid.initialize({"
      "  startOnLoad: false,"
      "  theme: 'default',"
      "  htmlLabels: false,"
      "  flowchart: { htmlLabels: false },"
      "  class: { htmlLabels: false },"
      "  state: { htmlLabels: false },"
      "  er: { useMaxWidth: true }"
      "});";
  if (!EvalGlobal(QByteArray(kInit), "init")) {
    return QString();
  }

  const QByteArray code_utf8 = code.toUtf8();
  JSValue global = JS_GetGlobalObject(g_context);
  JS_SetPropertyStr(
      g_context, global, "__code",
      JS_NewStringLen(g_context, code_utf8.constData(), code_utf8.size()));
  JS_SetPropertyStr(g_context, global, "__renderId",
                    JS_NewString(g_context, "m0"));
  JS_FreeValue(g_context, global);

  // 完成判据是显式的 __renderSettled：r.svg 缺失时 undefined 的细节不该让
  // C 侧去猜——JS 侧自己把 settled 置真，结果统一成字符串（svg || ''）
  static const char* const kCall =
      "globalThis.__renderResult = null;\n"
      "globalThis.__renderError = null;\n"
      "globalThis.__renderSettled = false;\n"
      "mermaid.render(__renderId, __code)\n"
      "  .then(r => { globalThis.__renderResult = r.svg || '';\n"
      "               globalThis.__renderSettled = true; })\n"
      "  .catch(e => { globalThis.__renderError = "
      "      (e && e.name ? e.name + ': ' + e.message : String(e));\n"
      "                 globalThis.__renderSettled = true; });";
  if (!EvalGlobal(QByteArray(kCall), "render-call")) {
    return QString();
  }

  // 上界与 mermaidx 同值：mindmap 这类图会不停排 rAF，靠它兜底。
  // 光有 job 数不够——"小步慢排"的图永远到不了上限但 UI 照样卡死，
  // 加墙钟 30 秒兜底
  constexpr int kMaxJobs = 200000;
  constexpr qint64 kPumpDeadlineMs = 30000;
  QElapsedTimer pump_clock;
  pump_clock.start();
  bool done = false;
  for (int i = 0; i < kMaxJobs; ++i) {
    JSContext* pump_context = nullptr;
    const int result = JS_ExecutePendingJob(g_runtime, &pump_context);
    if (result < 0) {
      JSValue exception = JS_GetException(g_context);
      const char* message = JS_ToCString(g_context, exception);
      qWarning() << "mermaid: pump job exception:"
                 << (message != nullptr ? message : "?");
      if (message != nullptr) {
        JS_FreeCString(g_context, message);
      }
      JS_FreeValue(g_context, exception);
      break;
    }
    if (result == 0) {
      break;
    }
    if (pump_clock.elapsed() > kPumpDeadlineMs) {
      qWarning() << "mermaid: pump exceeded" << kPumpDeadlineMs << "ms";
      break;
    }
    global = JS_GetGlobalObject(g_context);
    JSValue settled = JS_GetPropertyStr(g_context, global, "__renderSettled");
    // 只看显式的 settled 标志（JS 侧 then/catch 都置真）——结果与错误
    // 值本身的 null/undefined 细节不再参与判据
    done = JS_ToBool(g_context, settled) == 1;
    JS_FreeValue(g_context, settled);
    JS_FreeValue(g_context, global);
    if (done) {
      break;
    }
  }
  if (!done) {
    qWarning() << "mermaid: render did not settle within job budget";
    return QString();
  }

  global = JS_GetGlobalObject(g_context);
  JSValue error = JS_GetPropertyStr(g_context, global, "__renderError");
  if (!JS_IsNull(error) && !JS_IsUndefined(error)) {
    const char* message = JS_ToCString(g_context, error);
    qWarning() << "mermaid render error:"
               << (message != nullptr ? message : "?");
    if (message != nullptr) {
      JS_FreeCString(g_context, message);
    }
    JS_FreeValue(g_context, error);
    JS_FreeValue(g_context, global);
    return QString();
  }
  JS_FreeValue(g_context, error);

  QString svg;
  JSValue value = JS_GetPropertyStr(g_context, global, "__renderResult");
  if (!JS_IsNull(value) && !JS_IsUndefined(value)) {
    const char* text = JS_ToCString(g_context, value);
    svg = QString::fromUtf8(text != nullptr ? text : "");
    if (text != nullptr) {
      JS_FreeCString(g_context, text);
    }
  }
  JS_FreeValue(g_context, value);
  JS_FreeValue(g_context, global);
  return svg;
}

// 后处理三层，顺序不可颠倒：CleanSvg（文本定标）→ InlineSvgCss（CSS 内联 +
// 字体族强制）→ InlineSvgMarkers（marker 展开，颜色来自上一步内联的规则）
QString SvgToImgTag(const QString& svg) {
  static int dump_index = 0;
  const int index = dump_index++;

  // mermaid 的容器里可能还有别的，只留 <svg>...</svg>
  const int start = svg.indexOf(QLatin1String("<svg"));
  const int end = svg.lastIndexOf(QLatin1String("</svg>"));
  if (start < 0 || end < 0) {
    return QString();
  }
  const QString bare = svg.mid(start, end - start + 6);

  const QString cleaned = CleanSvg(bare);
  const QString inlined = InlineSvgCss(cleaned, MermaidFontFamily());
  const QString marked = InlineSvgMarkers(inlined);
  DumpStage(index, QStringLiteral("raw.svg"), bare.toUtf8());
  DumpStage(index, QStringLiteral("clean.svg"), cleaned.toUtf8());
  DumpStage(index, QStringLiteral("inlined.svg"), inlined.toUtf8());
  DumpStage(index, QStringLiteral("marked.svg"), marked.toUtf8());
  if (marked.isEmpty()) {
    return QString();
  }

  // 尺寸从 viewBox 读：mermaid 的根元素写的是 width="100%"
  static const QRegularExpression kViewBox(
      QStringLiteral("viewBox=\"([-\\d.]+) ([-\\d.]+) ([\\d.]+) ([\\d.]+)\""));
  const QRegularExpressionMatch match = kViewBox.match(marked);
  qreal width = 0.0;
  qreal height = 0.0;
  if (match.hasMatch()) {
    width = match.captured(3).toDouble();
    height = match.captured(4).toDouble();
  }
  if (width < 1.0 || height < 1.0) {
    QSvgRenderer probe(bare.toUtf8());
    const QSize size = probe.defaultSize();
    width = size.width();
    height = size.height();
  }
  if (width < 1.0 || height < 1.0) {
    return QString();
  }

  // 2 倍超采样、按 1 倍尺寸摆放：连线与箭头的边缘才是锐的。
  // Format_ARGB32 是 4 字节/像素，大图 2 倍会内存爆（5000×5000 的图 2 倍
  // = 1.6GB）：像素总量封顶 4096×4096（≈67MB 一张），超了自动降倍数
  QSvgRenderer renderer(marked.toUtf8());
  if (!renderer.isValid()) {
    qWarning() << "mermaid: QSvgRenderer rejected the cleaned svg";
    return QString();
  }
  qreal scale = 2.0;
  constexpr qreal kMaxRasterPixels = 4096.0 * 4096.0;
  const qreal area = width * height;
  if (area * scale * scale > kMaxRasterPixels) {
    scale = std::sqrt(kMaxRasterPixels / area);
  }
  QImage image(qRound(width * scale), qRound(height * scale),
               QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  renderer.render(&painter, QRectF(0.0, 0.0, width * scale, height * scale));
  painter.end();

  QBuffer buffer;
  buffer.open(QIODevice::WriteOnly);
  if (!image.save(&buffer, "PNG")) {
    return QString();  // PNG 编不出：返回空，原图位退回代码块
  }
  DumpStage(index, QStringLiteral("png"), buffer.data());

  return QStringLiteral(
             "<img class=\"mermaid\" src=\"data:image/png;base64,%1\" "
             "width=\"%2\" height=\"%3\">")
      .arg(QString::fromLatin1(buffer.data().toBase64()))
      .arg(qRound(width))
      .arg(qRound(height));
}

}  // namespace

bool MermaidRendererReady() { return EnsureLoaded(); }

void ClearMermaidCache() {
  g_cache.clear();
  g_cache_order.clear();
}

QString MermaidFontFamily() {
  static const QString family = PickCjkFamily();
  return family;
}

QString MermaidToImgTag(const QString& code) {
  const auto cached = g_cache.constFind(code);
  if (cached != g_cache.cend()) {
    return cached.value();
  }
  const QString svg = RenderMermaidSvg(code);
  const QString tag = svg.isEmpty() ? QString() : SvgToImgTag(svg);
  CacheInsert(code, tag);
  return tag;
}
