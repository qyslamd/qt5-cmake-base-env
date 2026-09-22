#ifndef FILE_GUIDANCE_SUPPORT_H_
#define FILE_GUIDANCE_SUPPORT_H_

#include <QPointer>
#include <QString>
#include <QVariant>
#include <QWidget>

class QMainWindow;
class GuidancePresentation;
class GuidanceController;
class GuidanceHomePage;

/*

TODO: 让funq的执行能被看到。此类用于转接guidance。
如果把guidance当成最后一步。考虑接入这个。纯视觉layer高亮它操作的控件，保留气泡，不要任何按钮，不用QDialog的exec。完全作为funq的执行步骤定位高亮部件，这样做可以同时承担引导和自动化测试
改造 GuidancePresentation。核心是加一个被动模式，让它能被外部驱动

setPassiveMode(true) 做三件事：

skip_btn_ / prev_btn_ / next_btn_ 全 hide()

count_down_timer_->stop()

mousePressEvent 里点遮罩退出的逻辑加一个 if (passive_mode_) return;

加 GuidanceBridge 这是  funq 和 presentation 之间的适配层。
模态对话框的 reparent
你的 presentation 现在挂在
viewport_（主窗口）上。模态对话框打开时，主窗口被挡，高亮看不见。
在 MainWindow（或 main.cpp）里创建 bridge 并挂到能被 funq 找到的位置
Python 侧封装

class GuidanceClient:
    def __init__(self, funq_client):
        self.funq = funq_client

    def highlight(self, path, title, text):
        self.funq.call_slot(
            path="MainWindow::guidanceBridge",
            slot_name="showHighlight",
            params={"path": path, "title": title, "text": text},
        )

    def clear(self):
        self.funq.call_slot(path="MainWindow::guidanceBridge",
slot_name="hideHighlight")


guide = GuidanceClient(client)
try:
    guide.highlight(LOGIN_BUTTON, "点击登录", "用错误密码试探")
    client.widget(path=LOGIN_PASSWORD).set_property("text", BAD_PASSWORD)
    client.widget(path=LOGIN_BUTTON).click()
    # ...
finally:
    guide.clear()

bridge 能被调到：Python 调 hideHighlight，看 C++ 侧有没有打印。这一步验证 funq →
bridge 的通道。

路径解析对：调 showHighlight 传一个已知路径（比如 loginButton），看 C++ 侧能不能
findObject 到。

高亮画出来：看屏幕上有没有遮罩 + 虚线框。

模态场景：在登录对话框上高亮，看 reparent 逻辑对不对。

接实际脚本：把 funq_client_demo.py 的每一步加上 highlight 调用。
*/
class GuidanceSupport : public QWidget {
  Q_OBJECT

 public:
  // controller 由调用方持有，本类只引用它：演示层挂在 controller 里，
  // 主题也由它管理，所以依赖一个对象就够，不必分别传进来。
  //
  // 基类是 QWidget 而不是 QObject，这是 funq 的要求：它按 path 定位之后要
  // qobject_cast<QWidget*>（WidgetLocatorContext），纯 QObject 会被判成
  // NotAWidget；而且客户端默认还要等控件 visible。本类没有任何界面，做成
  // 0 尺寸、不挡鼠标的一块占位控件
  explicit GuidanceSupport(GuidanceController* controller,
                           QWidget* parent = nullptr);

  // 在主窗口菜单栏挂 引导 菜单
  void attachMenu(QMainWindow* window);

  // funq 桥接。签名受 funq 限制：它固定按 QMetaObject::invokeMethod(
  // obj, name, DirectConnection, Q_RETURN_ARG(QVariant), Q_ARG(QVariant))
  // 调用，所以**必须**是"单个 QVariant 进、QVariant 出"——拆成多个形参
  // invokeMethod 匹配不上，会直接返回 NoMethodInvoked。
  // 参数用 map：{"path": ..., "title": ..., "text": ...}
  // 返回 {"ok": bool, "message": str}，脚本侧可据此判断
  Q_INVOKABLE QVariant showHighlight(const QVariant& params);
  Q_INVOKABLE QVariant hideHighlight(const QVariant& params);

 private:
  GuidanceController* controller_ = nullptr;
  QPointer<GuidancePresentation> presentation_;
  // 引导主页：懒建一次后复用，父级是主窗口，随窗口销毁
  QPointer<GuidanceHomePage> home_page_;
};

#endif  // FILE_GUIDANCE_SUPPORT_H_