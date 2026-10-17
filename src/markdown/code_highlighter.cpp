#include "code_highlighter.h"

#include <quickjs.h>

#include <QDebug>
#include <QFile>

#include "html_escape.h"
#include "markdown_resources.h"

namespace {

JSRuntime* g_runtime = nullptr;
JSContext* g_context = nullptr;
// 载荷里的 globalThis.__highlight，加载完成后拿一次引用；每次高亮直接
// JS_Call，不再现译一段 IIFE
JSValue g_highlight_fn = JS_UNDEFINED;
bool g_load_failed = false;

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
  return false;
}

bool EnsureLoaded() {
  if (g_context != nullptr) {
    return true;
  }
  if (g_load_failed) {
    return false;
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
  return true;
}

}  // namespace

QString HighlightCode(const QString& code, const QString& language) {
  if (code.isEmpty()) {
    return {};
  }
  if (!EnsureLoaded()) {
    return EscapeHtml(code);  // 载荷挂了：纯转义兜底
  }

  // 参数直接以 JS 字符串传入：不用手写 JS 源、不用全局变量，也不牵扯转义
  const QByteArray code_utf8 = code.toUtf8();
  const QByteArray lang_utf8 = language.toUtf8();
  JSValue argv[2] = {
      JS_NewStringLen(g_context, code_utf8.constData(), code_utf8.size()),
      JS_NewStringLen(g_context, lang_utf8.constData(), lang_utf8.size()),
  };
  const JSValue value =
      JS_Call(g_context, g_highlight_fn, JS_UNDEFINED, 2, argv);
  JS_FreeValue(g_context, argv[0]);
  JS_FreeValue(g_context, argv[1]);

  QString result;
  if (JS_IsException(value)) {
    JSValue exception = JS_GetException(g_context);
    const char* message = JS_ToCString(g_context, exception);
    qWarning() << "highlight call exception:"
               << (message != nullptr ? message : "?");
    if (message != nullptr) {
      JS_FreeCString(g_context, message);
    }
    JS_FreeValue(g_context, exception);
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

  // 空结果（未注册语言、JS 侧异常）都退纯转义：高亮挂了不至于白块
  if (result.isEmpty()) {
    return EscapeHtml(code);
  }
  return result;
}
