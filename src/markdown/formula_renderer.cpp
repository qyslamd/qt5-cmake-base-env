#include "formula_renderer.h"

#include <quickjs.h>

#include <QBuffer>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>
#include <QSvgRenderer>

#include "default_render_context.h"
#include "markdown_render_context_internal.h"
#include "markdown_resources.h"
#include "renderer_status_internal.h"

namespace {

JSRuntime* g_runtime = nullptr;
JSContext* g_context = nullptr;
bool g_load_failed = false;
bool g_load_attempted = false;
qint64 g_first_load_ms = 0;
QElapsedTimer g_load_clock;

// 载荷在 qrc 里（resources/mathjax-svg.min.js），只求值一次
// 加载失败：把 JS 运行时整个还回去。g_load_failed 已经挡住重试，留着
// context/runtime 只会占着 JS 堆到进程结束
bool FailLoad() {
  if (g_context != nullptr) {
    JS_FreeContext(g_context);
    g_context = nullptr;
  }
  if (g_runtime != nullptr) {
    JS_FreeRuntime(g_runtime);
    g_runtime = nullptr;
  }
  g_load_failed = true;
  g_first_load_ms = g_load_clock.isValid() ? g_load_clock.elapsed() : 0;
  return false;
}

bool EnsureLoaded() {
  if (g_context != nullptr) {
    return true;
  }
  if (g_load_failed) {
    return false;
  }

  if (!g_load_attempted) {
    g_load_attempted = true;
    g_load_clock.start();
  }

  EnsureMarkdownResources();
  QFile file(QStringLiteral(":/markdown/mathjax-svg.min.js"));
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "mathjax payload missing from resources";
    return FailLoad();
  }
  const QByteArray code = file.readAll();
  file.close();

  g_runtime = JS_NewRuntime();
  if (g_runtime == nullptr) {
    qWarning() << "JS_NewRuntime failed for mathjax payload";
    return FailLoad();
  }
  // MathJax 解析递归深，默认配额不够。这里放大 JS 侧配额，链接选项再把线程栈
  // 放大——两个数要配对，且**线程栈必须比 JS 配额宽裕**：JS 配额是"软"上限
  // （超了抛可捕获的异常），线程栈是"硬"的（超了直接崩，没有异常没有日志）。
  // 32 位目标上不能贪大：主线程栈 reserve 会吃掉 2GB 地址空间里的一大块，
  // 实测 128MB 时 funq 注入（CreateRemoteThread 要在目标进程里开线程栈）
  // 直接失败，报 WinError 8 内存资源不足
  JS_SetMaxStackSize(g_runtime, 16U << 20);
  g_context = JS_NewContext(g_runtime);
  if (g_context == nullptr) {
    qWarning() << "JS_NewContext failed for mathjax payload";
    return FailLoad();
  }

  const JSValue result = JS_Eval(g_context, code.constData(), code.size(),
                                 "mathjax-svg", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JSValue exception = JS_GetException(g_context);
    const char* message = JS_ToCString(g_context, exception);
    qWarning() << "mathjax payload eval failed:" << (message ? message : "?");
    if (message != nullptr) {
      JS_FreeCString(g_context, message);
    }
    JS_FreeValue(g_context, exception);
    JS_FreeValue(g_context, result);
    return FailLoad();
  }
  JS_FreeValue(g_context, result);
  g_first_load_ms = g_load_clock.elapsed();
  return true;
}

// SVG 字符串 → <img>。失败返回空串。color 有效时替换 currentColor
QString SvgToImgTag(const QString& svg, bool display, const QColor& color) {
  // MathJax 的输出包在 <mjx-container> 里，而 QSvgRenderer 只认 svg 根元素
  const int svg_start = svg.indexOf(QLatin1String("<svg"));
  const int svg_end = svg.lastIndexOf(QLatin1String("</svg>"));
  if (svg_start < 0 || svg_end < 0) {
    return {};
  }
  QString bare = svg.mid(svg_start, svg_end - svg_start + 6);
  if (color.isValid()) {
    bare.replace(QLatin1String("currentColor"), color.name());
  }

  QSvgRenderer renderer(bare.toUtf8());
  if (!renderer.isValid()) {
    return {};
  }

  // MathJax 的 width/height 是 ex 单位；打包时约定 em=16px、ex=8px
  static const QRegularExpression kSizeExpression(
      QStringLiteral("width=\"([\\d.]+)ex\"\\s+height=\"([\\d.]+)ex\""));
  const QRegularExpressionMatch size = kSizeExpression.match(bare);
  qreal width = 0.0;
  qreal height = 0.0;
  if (size.hasMatch()) {
    width = size.captured(1).toDouble() * 8.0;
    height = size.captured(2).toDouble() * 8.0;
  } else {
    const QSize fallback = renderer.defaultSize();
    width = fallback.width() / 1000.0 * 16.0;
    height = fallback.height() / 1000.0 * 16.0;
  }
  if (width < 1.0 || height < 1.0) {
    return {};
  }

  // 2 倍超采样、按 1 倍尺寸摆放：公式里的分数线与根号才有锐边
  QImage image(qRound(width * 2.0), qRound(height * 2.0),
               QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  renderer.render(&painter, QRectF(0.0, 0.0, width * 2.0, height * 2.0));
  painter.end();

  QBuffer buffer;
  buffer.open(QIODevice::WriteOnly);
  if (!image.save(&buffer, "PNG")) {
    return {};  // PNG 编不出：返回空，正文落回原文而不是坏 img
  }

  // data URI 内联：自定义 scheme 会被 qlitehtml 当主机名做浏览器式扩展
  // （实测 mj://0 变成 mj://0.0.0.0），走 data URI 完全不经 URL 解析
  return QStringLiteral(
             "<img%1 src=\"data:image/png;base64,%2\" width=\"%3\" "
             "height=\"%4\">")
      .arg(display ? QStringLiteral(" class=\"math-display\"") : QString(),
           QString::fromLatin1(buffer.data().toBase64()))
      .arg(qRound(width))
      .arg(qRound(height));
}

}  // namespace

bool FormulaRendererReady() { return EnsureLoaded(); }

MarkdownEngineStatus FormulaEngineStatus() {
  MarkdownEngineStatus status;
  status.loaded = g_context != nullptr;
  status.permanently_failed = g_load_failed;
  status.first_load_ms = g_first_load_ms;
  return status;
}

void ClearFormulaCache() {
  MarkdownRenderContextAccess::State(DefaultRenderContext())
      .formula_cache.Clear();
}

QString FormulaToImgTag(MarkdownRenderContext& context, const QString& tex,
                        bool display, const QColor& color) {
  MarkdownRenderContextState& state =
      MarkdownRenderContextAccess::State(context);
  // MathJax 在数学模式下对首尾空白不敏感：key 与渲染输入统一用 trim 后的
  // 文本，避免 "x" 与 " x " 生成两条内容相同的缓存
  const QString normalized = tex.trimmed();
  const QString variant =
      color.isValid() ? color.name() : QStringLiteral("legacy");
  const QString key = (display ? QStringLiteral("D") : QStringLiteral("I")) +
                      variant + QLatin1Char('|') + normalized;
  if (const QString* cached = state.formula_cache.Find(key)) {
    ++state.formula_stats.memory_cache_hits;
    return *cached;
  }
  ++state.formula_stats.memory_cache_misses;
  ++state.formula_stats.renders;
  if (!EnsureLoaded()) {
    ++state.formula_stats.failures;
    return {};
  }

  // tex 要进单引号 JS 字符串：反斜杠、引号、换行都得转义。换行尤其不能漏——
  // 块级 $$ 是跨行收集的，裸换行会让 JS 直接语法错误，公式静默落到兜底
  QString escaped = normalized;
  escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
  escaped.replace(QLatin1Char('\''), QStringLiteral("\\'"));
  escaped.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
  escaped.replace(QLatin1Char('\r'), QStringLiteral("\\r"));

  // 包一层 try/catch：JS 异常与原生崩溃要分得清（前者能拿到 message）
  const QString call = QStringLiteral(
                           "try { 'SVGOK:' + MathJaxAdapter.convert('%1', %2) }"
                           " catch (e) { 'SVGERR: ' + "
                           "(e && e.message ? e.message : e) }")
                           .arg(escaped, display ? QStringLiteral("true")
                                                 : QStringLiteral("false"));
  const QByteArray utf8 = call.toUtf8();
  const JSValue value = JS_Eval(g_context, utf8.constData(), utf8.size(),
                                "convert", JS_EVAL_TYPE_GLOBAL);

  QString tag;
  if (JS_IsException(value)) {
    JSValue exception = JS_GetException(g_context);
    const char* message = JS_ToCString(g_context, exception);
    qWarning() << "formula convert exception:" << (message ? message : "?");
    if (message != nullptr) {
      JS_FreeCString(g_context, message);
    }
    JS_FreeValue(g_context, exception);
  } else {
    const char* raw = JS_ToCString(g_context, value);
    if (raw == nullptr) {
      // 分配失败等情况下 QuickJS 返回 null，并可能留下 pending 异常
      qWarning() << "formula convert: JS_ToCString returned null";
      JS_FreeValue(g_context, JS_GetException(g_context));
    } else {
      const QString converted = QString::fromUtf8(raw);
      JS_FreeCString(g_context, raw);
      if (converted.startsWith(QLatin1String("SVGOK:"))) {
        tag = SvgToImgTag(converted.mid(6), display, color);
      } else {
        qWarning() << "formula convert failed:" << converted;
      }
    }
  }
  JS_FreeValue(g_context, value);

  if (tag.isEmpty()) {
    ++state.formula_stats.failures;
  }
  state.formula_cache.Insert(key, tag);
  return tag;
}

QString FormulaToImgTag(MarkdownRenderContext& context, const QString& tex,
                        bool display) {
  return FormulaToImgTag(context, tex, display, QColor());
}

QString FormulaToImgTag(const QString& tex, bool display) {
  return FormulaToImgTag(DefaultRenderContext(), tex, display);
}
