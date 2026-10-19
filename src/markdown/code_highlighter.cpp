#include "code_highlighter.h"

#include <quickjs.h>

#include <QDebug>
#include <QElapsedTimer>

#include "default_render_context.h"
#include "html_escape.h"
#include "js_error.h"
#include "js_runtime.h"
#include "markdown_bytecode.h"
#include "markdown_render_context_internal.h"
#include "render_diagnostics.h"
#include "renderer_status_internal.h"

namespace {

JsRuntime g_engine;
// 载荷里的 globalThis.__highlight，加载完成后拿一次引用；每次高亮直接
// JS_Call，不再现译一段 IIFE
JSValue g_highlight_fn = JS_UNDEFINED;

void ResetHighlightFn(JSContext* context) {
  JS_FreeValue(context, g_highlight_fn);
  g_highlight_fn = JS_UNDEFINED;
}

bool EnsureLoaded() {
  if (g_engine.status().loaded) {
    return true;
  }
  if (g_engine.status().permanently_failed) {
    return false;
  }
  g_engine.set_on_fail(ResetHighlightFn);
  return g_engine.EnsureLoaded(
      {JsPayload{md_hl, md_hl_size, "highlight.min.js"}}, nullptr,
      [](JSContext* context) {
        // 只解析一次函数引用；载荷没导出 __highlight 说明资源版本对不上
        JSValue global = JS_GetGlobalObject(context);
        g_highlight_fn = JS_GetPropertyStr(context, global, "__highlight");
        JS_FreeValue(context, global);
        if (!JS_IsFunction(context, g_highlight_fn)) {
          qWarning() << "highlight payload lacks __highlight";
          return false;
        }
        return true;
      });
}

}  // namespace

MarkdownEngineStatus HighlightEngineStatus() { return g_engine.status(); }

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
  JSContext* js = g_engine.context();
  QElapsedTimer render_clock;
  render_clock.start();

  // 参数直接以 JS 字符串传入：不用手写 JS 源、不用全局变量，也不牵扯转义
  const QByteArray code_utf8 = code.toUtf8();
  const QByteArray lang_utf8 = language.toUtf8();
  JSValue argv[2] = {
      JS_NewStringLen(js, code_utf8.constData(), code_utf8.size()),
      JS_NewStringLen(js, lang_utf8.constData(), lang_utf8.size()),
  };
  if (JS_IsException(argv[0]) || JS_IsException(argv[1])) {
    // 分配失败返回的是异常哨兵，不能当参数传给 JS_Call；取出并清掉
    // pending 异常，退回纯转义
    const QString message = TakeExceptionMessage(js);
    qWarning() << "highlight argument alloc failed:" << message;
    JS_FreeValue(js, argv[0]);
    JS_FreeValue(js, argv[1]);
    ++state.highlight_stats.failures;
    if (diagnostics.trace) {
      TraceRender(diagnostics,
                  QStringLiteral("highlight argument alloc failed"));
    }
    return EscapeHtml(code);
  }
  const JSValue value = JS_Call(js, g_highlight_fn, JS_UNDEFINED, 2, argv);
  JS_FreeValue(js, argv[0]);
  JS_FreeValue(js, argv[1]);

  QString result;
  QString error;
  if (!CheckResult(js, value, QStringLiteral("highlight call"), &error)) {
    ++state.highlight_stats.failures;
    if (diagnostics.trace) {
      TraceRender(diagnostics,
                  QStringLiteral("highlight exception: %1").arg(error));
    }
  } else if (JS_IsNull(value) || JS_IsUndefined(value)) {
    // __highlight 对未注册语言返回 null；JS_ToCString 会把它变成字面 "null"
  } else {
    const char* text = JS_ToCString(js, value);
    result = QString::fromUtf8(text != nullptr ? text : "");
    if (text != nullptr) {
      JS_FreeCString(js, text);
    }
  }
  JS_FreeValue(js, value);

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
