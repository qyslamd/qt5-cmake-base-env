#include "js_error.h"

#include <cstdint>
#include <gtest/gtest.h>

#include <QString>

// QuickJS 错误处理助手的判据：异常消息真的被消费、坏字节码不崩且报错、
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

TEST(JsErrorTest, TakesPendingExceptionMessage) {
  ScopedJsContext js;
  ASSERT_NE(js.context(), nullptr);
  JS_ThrowTypeError(js.context(), "boom");
  const QString message = TakeExceptionMessage(js.context());
  EXPECT_TRUE(message.contains(QStringLiteral("boom")))
      << message.toStdString();
}

TEST(JsErrorTest, CorruptBytecodeReportsError) {
  ScopedJsContext js;
  ASSERT_NE(js.context(), nullptr);
  const uint8_t garbage[] = {0xde, 0xad, 0xbe, 0xef};
  QString error;
  EXPECT_FALSE(EvalBytecode(js.context(), garbage, sizeof(garbage),
                            QStringLiteral("garbage"), &error));
  EXPECT_FALSE(error.isEmpty());
}

TEST(JsErrorTest, CheckResultAcceptsNonException) {
  ScopedJsContext js;
  ASSERT_NE(js.context(), nullptr);
  const JSValue value = JS_NewInt32(js.context(), 1);
  QString error;
  EXPECT_TRUE(
      CheckResult(js.context(), value, QStringLiteral("probe"), &error));
  EXPECT_TRUE(error.isEmpty());
  JS_FreeValue(js.context(), value);
}

}  // namespace
