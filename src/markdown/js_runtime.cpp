#include "js_runtime.h"

#include <QDebug>
#include <QElapsedTimer>

#include "js_error.h"

namespace {

// 与 formula/mermaid 的递归深度配对：JS 配额是软上限，宿主线程栈是硬的
constexpr size_t kJsMaxStackSize = 16U << 20;

}  // namespace

JsRuntime::~JsRuntime() { FailLoad(); }

void JsRuntime::set_on_fail(std::function<void(JSContext*)> on_fail) {
  on_fail_ = std::move(on_fail);
}

void JsRuntime::set_preserve_first_load(bool preserve) {
  preserve_first_load_ = preserve;
}

bool JsRuntime::EnsureLoaded(
    const QList<JsPayload>& payloads,
    const std::function<bool(JSContext*)>& prepare,
    const std::function<bool(JSContext*)>& after_load) {
  if (context_ != nullptr) {
    return true;
  }
  if (load_failed_) {
    return false;
  }
  if (!load_attempted_) {
    load_attempted_ = true;
    load_clock_.start();
  }

  runtime_ = JS_NewRuntime();
  if (runtime_ == nullptr) {
    return FailLoad();
  }
  JS_SetMaxStackSize(runtime_, kJsMaxStackSize);
  context_ = JS_NewContext(runtime_);
  if (context_ == nullptr) {
    return FailLoad();
  }

  if (prepare && !prepare(context_)) {
    return FailLoad();
  }
  for (const JsPayload& payload : payloads) {
    QString error;
    if (!EvalBytecode(context_, payload.data, payload.size,
                      QString::fromUtf8(payload.name), &error)) {
      return FailLoad();
    }
  }
  if (after_load && !after_load(context_)) {
    return FailLoad();
  }

  first_load_ms_ = load_clock_.elapsed();
  return true;
}

bool JsRuntime::Eval(const QByteArray& code, const char* script_name,
                     const QString& label) {
  if (context_ == nullptr) {
    qWarning() << label << "failed: no JS context";
    return false;
  }
  const JSValue value = JS_Eval(context_, code.constData(), code.size(),
                                script_name, JS_EVAL_TYPE_GLOBAL);
  QString error;
  const bool ok = CheckResult(context_, value, label, &error);
  JS_FreeValue(context_, value);
  return ok;
}

MarkdownEngineStatus JsRuntime::status() const {
  MarkdownEngineStatus status;
  status.loaded = context_ != nullptr;
  status.permanently_failed = load_failed_;
  status.first_load_ms = first_load_ms_;
  return status;
}

void JsRuntime::Abort() { FailLoad(); }

bool JsRuntime::FailLoad() {
  if (context_ != nullptr) {
    if (on_fail_) {
      on_fail_(context_);
    }
    JS_FreeContext(context_);
    context_ = nullptr;
  }
  if (runtime_ != nullptr) {
    JS_FreeRuntime(runtime_);
    runtime_ = nullptr;
  }
  load_failed_ = true;
  // mermaid 的主题 initialize 失败也会走到这里：保留已经测到的首次加载耗时；
  // 其他引擎沿用"失败时刻覆盖"的旧行为
  if (!preserve_first_load_ || first_load_ms_ == 0) {
    first_load_ms_ = load_clock_.isValid() ? load_clock_.elapsed() : 0;
  }
  return false;
}
