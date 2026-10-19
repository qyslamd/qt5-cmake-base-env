#include "js_error.h"

#include <QDebug>

QString TakeExceptionMessage(JSContext* context) {
  if (context == nullptr) {
    return QStringLiteral("no JS context");
  }
  const JSValue exception = JS_GetException(context);
  const char* raw = JS_ToCString(context, exception);
  const QString message = QString::fromUtf8(raw != nullptr ? raw : "?");
  if (raw != nullptr) {
    JS_FreeCString(context, raw);
  }
  JS_FreeValue(context, exception);
  return message;
}

bool CheckResult(JSContext* context, JSValue value, const QString& label,
                 QString* error) {
  if (!JS_IsException(value)) {
    return true;
  }
  const QString message = TakeExceptionMessage(context);
  qWarning() << label << "failed:" << message;
  if (error != nullptr) {
    *error = message;
  }
  return false;
}

bool EvalBytecode(JSContext* context, const uint8_t* data, uint32_t size,
                  const QString& name, QString* error) {
  JSValue obj = JS_ReadObject(context, data, size, JS_READ_OBJ_BYTECODE);
  if (JS_IsException(obj)) {
    const QString message = TakeExceptionMessage(context);
    qWarning() << "bytecode read failed:" << name << message;
    if (error != nullptr) {
      *error = message;
    }
    JS_FreeValue(context, obj);  // 异常哨兵，Free 是 no-op
    return false;
  }
  // EvalFunction 接管 obj 所有权，成功后不能再 Free obj
  const JSValue value = JS_EvalFunction(context, obj);
  if (JS_IsException(value)) {
    const QString message = TakeExceptionMessage(context);
    qWarning() << "bytecode eval failed:" << name << message;
    if (error != nullptr) {
      *error = message;
    }
    JS_FreeValue(context, value);
    return false;
  }
  JS_FreeValue(context, value);
  return true;
}
