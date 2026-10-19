#ifndef FILE_MARKDOWN_JS_RUNTIME_H_
#define FILE_MARKDOWN_JS_RUNTIME_H_

#include <quickjs.h>

#include <functional>

#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QString>

#include "markdown_types.h"

// 一段 qjsc 生成的字节码载荷
struct JsPayload {
  const uint8_t* data = nullptr;
  uint32_t size = 0;
  const char* name = nullptr;
};

// 一个 JS 渲染引擎的运行时：runtime/context 生命周期、字节码加载、一次性
// 失败标志、首次加载耗时。渲染语义（trace/计数/错误文案）留在各渲染器，
// 这里只提供共同机制
class JsRuntime {
 public:
  JsRuntime() = default;
  ~JsRuntime();
  JsRuntime(const JsRuntime&) = delete;
  JsRuntime& operator=(const JsRuntime&) = delete;

  // prepare 在载荷之前执行（注册 C 函数/搭 bridge），after_load 在载荷之后
  // 执行（取函数引用/暴露全局）。任一回调返回 false 都会作废引擎
  bool EnsureLoaded(const QList<JsPayload>& payloads,
                    const std::function<bool(JSContext*)>& prepare,
                    const std::function<bool(JSContext*)>& after_load);

  // 执行一段脚本（mermaid 的 init/bridge 这类动态代码）
  bool Eval(const QByteArray& code, const char* script_name,
            const QString& label);

  JSContext* context() const { return context_; }
  // mermaid 泵微任务需要 runtime（JS_ExecutePendingJob）
  JSRuntime* runtime() const { return runtime_; }
  MarkdownEngineStatus status() const;

  // 释放 context 前调用：highlighter 释放函数引用，mermaid 复位 initialize 标志
  void set_on_fail(std::function<void(JSContext*)> on_fail);
  // true：首次加载耗时只在为 0 时写入（mermaid 语义）；默认失败也覆盖
  void set_preserve_first_load(bool preserve);
  // 载荷之后的自检失败（如 initialize 失败）也要作废引擎
  void Abort();

 private:
  bool FailLoad();

  JSRuntime* runtime_ = nullptr;
  JSContext* context_ = nullptr;
  bool load_failed_ = false;
  bool load_attempted_ = false;
  qint64 first_load_ms_ = 0;
  QElapsedTimer load_clock_;
  std::function<void(JSContext*)> on_fail_;
  bool preserve_first_load_ = false;
};

#endif  // FILE_MARKDOWN_JS_RUNTIME_H_
