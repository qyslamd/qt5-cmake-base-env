#ifndef FILE_MARKDOWN_JS_ERROR_H_
#define FILE_MARKDOWN_JS_ERROR_H_

#include <quickjs.h>

#include <cstdint>

#include <QString>

// 三个 QuickJS 渲染器共用的错误处理。只收共同机制：异常消息提取、
// 结果检查、qjsc 字节码加载；不碰各渲染器的 trace/计数/错误文案差异

// 消费当前 pending 异常并返回可读消息；内部负责 GetException/ToCString/
// FreeCString/FreeValue，调用方不要再碰这个异常
QString TakeExceptionMessage(JSContext* context);

// value 是异常哨兵时：取消息、qWarning 输出 "<label> failed: msg"、写 error，
// 返回 false；否则返回 true。value 的所有权仍在调用方（异常哨兵 Free 是 no-op）
bool CheckResult(JSContext* context, JSValue value, const QString& label,
                 QString* error);

// qjsc 字节码：ReadObject + EvalFunction。失败时 error 带原因；
// EvalFunction 成功后接管 obj 所有权，内部不再 Free obj
bool EvalBytecode(JSContext* context, const uint8_t* data, uint32_t size,
                  const QString& name, QString* error);

#endif  // FILE_MARKDOWN_JS_ERROR_H_
