#include "code_highlighter.h"

#include <quickjs.h>

#include <QDebug>
#include <QFile>

// 静态库里的 qrc 不会自动初始化，而载荷就在里面。Q_INIT_RESOURCE 展开的是
// extern 声明 + 一次调用，**必须留在文件作用域、且不能放进匿名命名空间**
// （放进去会绑到匿名空间上，链接期找不到 rcc 生成的全局函数）。重复调用幂等
static void EnsureResourcesInitialized() { Q_INIT_RESOURCE(markdown_res); }

namespace {

JSRuntime* g_runtime = nullptr;
JSContext* g_context = nullptr;
bool g_load_failed = false;

bool EnsureLoaded() {
  if (g_context != nullptr) {
    return true;
  }
  if (g_load_failed) {
    return false;
  }

  EnsureResourcesInitialized();
  QFile file(QStringLiteral(":/markdown/highlight.min.js"));
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "highlight payload missing from resources";
    g_load_failed = true;
    return false;
  }
  const QByteArray code = file.readAll();
  file.close();

  g_runtime = JS_NewRuntime();
  if (g_runtime == nullptr) {
    g_load_failed = true;
    return false;
  }
  // 高亮不递归得深，16MB 与 formula/mermaid 两个渲染器同一档
  JS_SetMaxStackSize(g_runtime, 16U << 20);
  g_context = JS_NewContext(g_runtime);
  if (g_context == nullptr) {
    g_load_failed = true;
    return false;
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
    g_load_failed = true;
    return false;
  }
  JS_FreeValue(g_context, result);
  return true;
}

QString EscapeHtml(const QString& s) {
  QString out = s;
  out.replace(QChar('&'), QStringLiteral("&amp;"));
  out.replace(QChar('<'), QStringLiteral("&lt;"));
  out.replace(QChar('>'), QStringLiteral("&gt;"));
  return out;
}

}  // namespace

QString HighlightCode(const QString& code, const QString& language) {
  if (code.isEmpty()) {
    return {};
  }
  if (!EnsureLoaded()) {
    return EscapeHtml(code);  // 载荷挂了：纯转义兜底
  }

  // 参数走全局变量传入，避免手写 JS 字符串转义（代码块里什么字符都可能有）
  const QByteArray code_utf8 = code.toUtf8();
  const QByteArray lang_utf8 = language.toUtf8();
  JSValue global = JS_GetGlobalObject(g_context);
  JS_SetPropertyStr(
      g_context, global, "__hl_code",
      JS_NewStringLen(g_context, code_utf8.constData(), code_utf8.size()));
  JS_SetPropertyStr(
      g_context, global, "__hl_lang",
      JS_NewStringLen(g_context, lang_utf8.constData(), lang_utf8.size()));
  JS_FreeValue(g_context, global);

  static const QByteArray kCall = QByteArrayLiteral(
      "(function(){"
      "  var r = __highlight(__hl_code, __hl_lang);"
      "  return (r === null || r === undefined) ? '' : r;"
      "})()");
  const JSValue value = JS_Eval(g_context, kCall.constData(), kCall.size(),
                                "hl-call", JS_EVAL_TYPE_GLOBAL);

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
