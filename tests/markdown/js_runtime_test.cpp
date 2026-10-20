#include "js_runtime.h"

#include <cstdint>
#include <gtest/gtest.h>

#include <QString>

// JsRuntime 错误处理的判据：异常消息真的被消费、坏字节码不崩且作废引擎、
// 非异常结果原样放行
namespace {

class ScopedJsContext {
 public:
  ScopedJsContext() : runtime_(JS_NewRuntime()) {
    if (runtime_ != nullptr) {
      context_ = JS_NewContext(runtime_);
    }
  }
  ~ScopedJsContext() {
    if (context_ != nullptr) {
      JS_FreeContext(context_);
    }
    if (runtime_ != nullptr) {
      JS_FreeRuntime(runtime_);
    }
  }
  ScopedJsContext(const ScopedJsContext&) = delete;
  ScopedJsContext& operator=(const ScopedJsContext&) = delete;
  JSContext* context() const { return context_; }

 private:
  JSRuntime* runtime_ = nullptr;
  JSContext* context_ = nullptr;
};

TEST(JsRuntimeTest, TakesPendingExceptionMessage) {
  ScopedJsContext js;
  ASSERT_NE(js.context(), nullptr);
  JS_ThrowTypeError(js.context(), "boom");
  const QString message = JsRuntime::TakeExceptionMessage(js.context());
  EXPECT_TRUE(message.contains(QStringLiteral("boom")))
      << message.toStdString();
}

TEST(JsRuntimeTest, CorruptBytecodeFailsLoad) {
  const uint8_t garbage[] = {0xde, 0xad, 0xbe, 0xef};
  const QList<JsPayload> payloads = {
      JsPayload{garbage, static_cast<uint32_t>(sizeof(garbage)), "garbage"}};
  JsRuntime runtime;
  EXPECT_FALSE(runtime.EnsureLoaded(payloads, {}, {}));
  EXPECT_TRUE(runtime.status().permanently_failed);
}

TEST(JsRuntimeTest, CheckResultAcceptsNonException) {
  ScopedJsContext js;
  ASSERT_NE(js.context(), nullptr);
  const JSValue value = JS_NewInt32(js.context(), 1);
  QString error;
  EXPECT_TRUE(JsRuntime::CheckResult(js.context(), value,
                                     QStringLiteral("probe"), &error));
  EXPECT_TRUE(error.isEmpty());
  JS_FreeValue(js.context(), value);
}

}  // namespace
