"""funq 客户端示例：把 demo_app 从登录一路驱动到设置对话框。

这是 funq 的「client 侧」。它不启动被测程序、不碰它的进程，只往一条 TCP
连接上发消息。被驱动的那一端要先用另一个终端跑起来：

    终端 1（server 侧）  <构建目录>/examples/funq/serve_demo_app.bat
    终端 2（client 侧）  <构建目录>/examples/funq/run_demo_client.bat

两边对应 funq 的两个包，谁也不依赖谁：

    funq_server  注入器 + libFunq，跑在被测程序那一侧
    funq         本脚本用的客户端 API，只负责发消息、收结果

注意 client 侧根本不知道被测程序叫什么、在哪个目录——它只认 host:port。
这正是「分离」的意思：被测程序可以是任何 Qt 程序，用例换个端口就能接管。

读完这个文件，funq 能做什么、断言怎么写，基本就清楚了。
"""

import argparse
import sys
import time

# Windows 下 stdout 不是控制台时（重定向、管道），Python 按系统区域编码输出
# （中文机器上是 GBK），而启动器已经把控制台切到 UTF-8——显式钉死 UTF-8，
# 免得窗口里正常、重定向后变乱码
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8")

# funq 的客户端包。自动化用例那边由 CMake 把路径塞进环境变量，这里手工跑，
# 所以自己找一下；找不到就报一句人话，别丢一个 ImportError 让人猜
try:
    from funq.client import FunqClient
except ImportError:
    sys.stderr.write(
        "找不到 funq 包。请用同目录的 run_demo_client.bat / .sh 启动本脚本，\n"
        "它们会把 funq 的 client 目录放进 PYTHONPATH。\n")
    sys.exit(2)

# ---------------------------------------------------------------------------
# 被测界面上的控件路径。demo_app 的控件都设了 objectName，而 funq 的路径就是
# 「父路径::自己的 objectName」逐层拼下来的（见 libFunq/objectpath.cpp 的
# _rawObjectName），所以这些路径可以直接从源码推出来，不用靠探针反推。
# 唯一要注意的是中间层：布局容器也会成为路径上的一段，不能只看 new 的父子关系。
# ---------------------------------------------------------------------------

LOGIN_USERNAME = "loginDialog::loginContent::loginForm::loginUsername"
LOGIN_PASSWORD = "loginDialog::loginContent::loginForm::loginPassword"
LOGIN_BUTTON = "loginDialog::loginContent::loginForm::loginButton"
LOGIN_HINT = "loginDialog::loginContent::loginForm::loginHint"

MAIN_WINDOW = "MainWindow"
MAIN_TITLE = "MainWindow::QWidget::pagePlaceholderTitle"

# 菜单项是 QAction，不是 QWidget，所以路径里带的是序号后缀（它们没有
# objectName）。「工具」菜单的第三个动作就是「设置」
SETTINGS_ACTION = "MainWindow::QMenuBar::QMenu-1::QAction-2"

SETTINGS_DIALOG = "MainWindow::settingsDialog"
SETTINGS_CLOSE = "MainWindow::settingsDialog::settingsCloseButton"

# 设置对话框的两个页面都设了 objectName（见 settings_dialog.cpp），所以路径
# 直接可推。
#
# 顺带记一笔反例：这两个页面原本是裸 QWidget，funq 便退回「类名 + 同名兄弟的
# 序号」，而那个序号与添加顺序是**反的**——页面先 new 出来、再被 addWidget
# reparent 进去，setParent 会把子对象插到 children() 列表前面，funq 按列表
# 顺序数号，「日志」页因此成了 QWidget-1。控件没有 objectName 时，路径就只能
# 实测。详见 examples/funq/README.md
SETTINGS_PAGE_LOG = "MainWindow::settingsDialog::settingsPageStack::settingsPageLog"
SETTINGS_RETAIN_DAYS = (
    SETTINGS_PAGE_LOG + "::settingsCard::settingsRow-1::QSpinBox")

GOOD_USER = "admin"
GOOD_PASSWORD = "123"
BAD_PASSWORD = "definitely-wrong"


def banner(step, title):
    print()
    print("=" * 70)
    print("第 %d 步：%s" % (step, title))
    print("=" * 70)


def brief(widget, keys=("text", "visible", "enabled", "checked", "value")):
    """挑几个好读的属性打印，免得刷屏。"""
    props = widget.properties()
    return {k: props[k] for k in keys if k in props}


def exists(client, path):
    """路径在不在（不等待、不要求可见）。"""
    try:
        client.widget(path=path, timeout=1.0, wait_active=False)
        return True
    except Exception:
        return False


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--host", default="localhost",
                        help="libFunq 监听的地址（默认 localhost）")
    parser.add_argument("--port", type=int, default=9000,
                        help="libFunq 监听的端口（默认 9000）")
    opts = parser.parse_args()

    banner(1, "连上去")
    print("目标：%s:%d" % (opts.host, opts.port))
    print("服务端没起来的话，这里会等 10 秒然后报 ConnectionRefusedError。")
    try:
        client = FunqClient(host=opts.host, port=opts.port)
    except Exception as exc:
        sys.stderr.write("\n连不上（%s）。\n"
                         "确认另一个终端里的 serve_demo_app 还在跑。\n" % exc)
        return 1
    print("连上了。注意：整个过程 client 没启动过任何程序。")

    banner(2, "看一眼登录界面上有什么")
    print("登录对话框在 funq 眼里长这样：")
    print("    %s" % LOGIN_USERNAME)
    print("    %s" % LOGIN_PASSWORD)
    print("    %s" % LOGIN_BUTTON)
    print("    %s   <- 提示文字在这里" % LOGIN_HINT)

    banner(3, "故意输错密码")
    client.widget(path=LOGIN_USERNAME).set_property("text", GOOD_USER)
    client.widget(path=LOGIN_PASSWORD).set_property("text", BAD_PASSWORD)
    client.widget(path=LOGIN_BUTTON).click()
    # 界面的反应不是同步的：点击只是把事件投进 Qt 队列，得给它一点时间
    time.sleep(1.0)
    hint = client.widget(path=LOGIN_HINT).properties()["text"]
    print("输错后，提示文字 = %r" % hint)
    print("此时主窗口在不在：%s（应该不在）" % exists(client, MAIN_WINDOW))
    assert hint == "用户名或密码错误", "提示文字与预期不符：%r" % hint
    print("断言通过：登录失败这条路走对了。")

    banner(4, "换正确密码，进主窗口")
    client.widget(path=LOGIN_PASSWORD).set_property("text", GOOD_PASSWORD)
    client.widget(path=LOGIN_BUTTON).click()
    # 登录成功后主窗口才创建，等一下它出来
    main_window = client.widget(path=MAIN_WINDOW, timeout=10.0)
    print("主窗口已出现：%s" % brief(main_window, ("visible", "enabled")))
    print("中央占位控件的标题：%r"
          % client.widget(path=MAIN_TITLE).properties()["text"])

    banner(5, "从菜单触发「设置」")
    print("菜单动作是 QAction，用 client.action() 取，trigger() 触发。")
    print("坑在这里：trigger(blocking=True) 会一直等到动作做完，而「设置」")
    print("开的是模态对话框、自己不会返回——所以必须 blocking=False。")
    action = client.action(path=SETTINGS_ACTION)
    print("拿到的动作：%r" % action.properties().get("text"))
    action.trigger(blocking=False)
    client.widget(path=SETTINGS_DIALOG, timeout=10.0)
    print("设置对话框出现了：%s" % SETTINGS_DIALOG)

    banner(6, "在设置对话框里读一个值")
    print("目标：%s" % SETTINGS_RETAIN_DAYS)
    print("（「日志」页的『保留天数』）")
    spin = client.widget(path=SETTINGS_RETAIN_DAYS)
    before = spin.properties()["value"]
    print("当前 value = %s" % before)

    print("改一个值再读回来——写属性走的是同一条 TCP 通道：")
    spin.set_property("value", before + 1)
    spin.wait_for_properties({"value": before + 1})
    print("  改完 value = %s" % spin.properties()["value"])

    print("改回原样，免得动到你的配置：")
    spin.set_property("value", before)
    spin.wait_for_properties({"value": before})
    print("  复原 value = %s" % spin.properties()["value"])

    banner(7, "收尾")
    client.widget(path=SETTINGS_CLOSE).click()
    time.sleep(0.5)
    print("设置对话框还在不在：%s（应该不在）" % exists(client, SETTINGS_DIALOG))
    client.close()
    print("连接已断开。被测程序还在跑——它归终端 1 管，Ctrl+C 才结束它。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
