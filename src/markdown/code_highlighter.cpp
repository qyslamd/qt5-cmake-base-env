#include "code_highlighter.h"

#include <quickjs.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>

#include "default_render_context.h"
#include "html_escape.h"
#include "markdown_render_context_internal.h"
#include "markdown_resources.h"
#include "render_diagnostics.h"
#include "renderer_status_internal.h"

namespace {

JSRuntime* g_runtime = nullptr;
JSContext* g_context = nullptr;
// 载荷里的 globalThis.__highlight，加载完成后拿一次引用；每次高亮直接
// JS_Call，不再现译一段 IIFE
JSValue g_highlight_fn = JS_UNDEFINED;
bool g_load_failed = false;
bool g_load_attempted = false;
qint64 g_first_load_ms = 0;
QElapsedTimer g_load_clock;

// 加载失败：把 JS 运行时整个还回去。g_load_failed 已经挡住重试，留着
// context/runtime 只会占着 JS 堆到进程结束
bool FailLoad() {
  if (g_context != nullptr) {
    JS_FreeValue(g_context, g_highlight_fn);
    g_highlight_fn = JS_UNDEFINED;
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
  QFile file(QStringLiteral(":/markdown/highlight.min.js"));
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "highlight payload missing from resources";
    return FailLoad();
  }
  const QByteArray code = file.readAll();
  file.close();

  g_runtime = JS_NewRuntime();
  if (g_runtime == nullptr) {
    return FailLoad();
  }
  // 高亮不递归得深，16MB 与 formula/mermaid 两个渲染器同一档
  JS_SetMaxStackSize(g_runtime, 16U << 20);
  g_context = JS_NewContext(g_runtime);
  if (g_context == nullptr) {
    return FailLoad();
  }

  const JSValue result = JS_Eval(g_context, code.constData(), code.size(),
                                 "highlight", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JSValue exception = JS_GetException(g_context);
    const char* message = JS_ToCString(g_context, exception);
    qWarning() << "highlight payload eval failed:"
               << (message != nullptr ? message : "?");
    if (message != nullptr) {
      JS_FreeCString(g_context, message);
    }
    JS_FreeValue(g_context, exception);
    JS_FreeValue(g_context, result);
    return FailLoad();
  }
  JS_FreeValue(g_context, result);

  // 只解析一次函数引用；载荷没导出 __highlight 说明资源与入口脚本版本对不上
  JSValue global = JS_GetGlobalObject(g_context);
  g_highlight_fn = JS_GetPropertyStr(g_context, global, "__highlight");
  JS_FreeValue(g_context, global);
  if (!JS_IsFunction(g_context, g_highlight_fn)) {
    qWarning() << "highlight payload lacks __highlight";
    return FailLoad();
  }
  g_first_load_ms = g_load_clock.elapsed();
  return true;
}

}  // namespace

MarkdownEngineStatus HighlightEngineStatus() {
  MarkdownEngineStatus status;
  status.loaded = g_context != nullptr;
  status.permanently_failed = g_load_failed;
  status.first_load_ms = g_first_load_ms;
  return status;
}

QString HighlightCode(MarkdownRenderContext& context, const QString& code,
                      const QString& language) {
  if (code.isEmpty()) {
    return {};
  }
  MarkdownRenderContextState& state =
      MarkdownRenderContextAccess::State(context);
  const MarkdownDiagnostics& diagnostics = state.options.diagnostics;
  ++state.highlight_stats.renders;
  const bool was_loaded = HighlightEngineStatus().loaded;
  if (!EnsureLoaded()) {
    ++state.highlight_stats.failures;
    if (diagnostics.trace) {
      TraceRender(diagnostics, QStringLiteral("highlight load failed"));
    }
    return EscapeHtml(code);  // 载荷挂了：纯转义兜底
  }
  if (!was_loaded && diagnostics.trace) {
    // 冷加载单独记账：first_load_ms 已经算好，别混进下面的渲染耗时
    TraceRender(diagnostics, QStringLiteral("highlight engine loaded in %1 ms")
                                 .arg(HighlightEngineStatus().first_load_ms));
  }
  QElapsedTimer render_clock;
  render_clock.start();

  // 参数直接以 JS 字符串传入：不用手写 JS 源、不用全局变量，也不牵扯转义
  const QByteArray code_utf8 = code.toUtf8();
  const QByteArray lang_utf8 = language.toUtf8();
  JSValue argv[2] = {
      JS_NewStringLen(g_context, code_utf8.constData(), code_utf8.size()),
      JS_NewStringLen(g_context, lang_utf8.constData(), lang_utf8.size()),
  };
  if (JS_IsException(argv[0]) || JS_IsException(argv[1])) {
    // 分配失败返回的是异常哨兵，不能当参数传给 JS_Call；取出并清掉
    // pending 异常，退回纯转义
    JSValue exception = JS_GetException(g_context);
    const char* message = JS_ToCString(g_context, exception);
    qWarning() << "highlight argument alloc failed:"
               << (message != nullptr ? message : "?");
    if (message != nullptr) {
      JS_FreeCString(g_context, message);
    }
    JS_FreeValue(g_context, exception);
    JS_FreeValue(g_context, argv[0]);
    JS_FreeValue(g_context, argv[1]);
    ++state.highlight_stats.failures;
    if (diagnostics.trace) {
      TraceRender(diagnostics,
                  QStringLiteral("highlight argument alloc failed"));
    }
    return EscapeHtml(code);
  }
  const JSValue value =
      JS_Call(g_context, g_highlight_fn, JS_UNDEFINED, 2, argv);
  JS_FreeValue(g_context, argv[0]);
  JS_FreeValue(g_context, argv[1]);

  QString result;
  if (JS_IsException(value)) {
    JSValue exception = JS_GetException(g_context);
    const char* raw_message = JS_ToCString(g_context, exception);
    const QString message =
        QString::fromUtf8(raw_message != nullptr ? raw_message : "?");
    qWarning() << "highlight call exception:" << message;
    if (raw_message != nullptr) {
      JS_FreeCString(g_context, raw_message);
    }
    JS_FreeValue(g_context, exception);
    ++state.highlight_stats.failures;
    if (diagnostics.trace) {
      TraceRender(diagnostics,
                  QStringLiteral("highlight exception: %1").arg(message));
    }
  } else if (JS_IsNull(value) || JS_IsUndefined(value)) {
    // __highlight 对未注册语言返回 null；JS_ToCString 会把它变成字面 "null"
  } else {
    const char* text = JS_ToCString(g_context, value);
    result = QString::fromUtf8(text != nullptr ? text : "");
    if (text != nullptr) {
      JS_FreeCString(g_context, text);
    }
  }
  JS_FreeValue(g_context, value);

  // 空结果（未注册语言、JS 侧异常）都退纯转义：高亮挂了不至于白块。
  // 未注册语言是正常回退，不计 failure
  if (result.isEmpty()) {
    if (diagnostics.trace) {
      TraceRender(
          diagnostics,
          QStringLiteral("highlight fallback: language=%1")
              .arg(language.isEmpty() ? QStringLiteral("text") : language));
    }
    return EscapeHtml(code);
  }
  if (diagnostics.trace) {
    TraceRender(diagnostics,
                QStringLiteral("highlight %1: %2 ms, %3 bytes")
                    .arg(language.isEmpty() ? QStringLiteral("text") : language)
                    .arg(render_clock.elapsed())
                    .arg(result.size()));
  }
  return result;
}

QString HighlightCode(const QString& code, const QString& language) {
  return HighlightCode(DefaultRenderContext(), code, language);
}
