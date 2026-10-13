"""funq 客户端示例：把 demo_app 从登录一路驱动到设置对话框。

登录开着（默认）走完整七步；登录在配置里关掉（login/enabled=false）或被
命令行 --set 关掉时没有登录对话框，脚本探测到之后跳过登录那三步，从主窗口
继续（第 5 步起）。

这是 funq 的「client 侧」。它不启动被测程序、不碰它的进程，只往一条 TCP
连接上发消息。被驱动的那一端要先用另一个终端跑起来：

    终端 1（server 侧）  <构建目录>/bin/funq-server-demo.bat
    终端 2（client 侧）  <构建目录>/bin/funq-client-demo.bat

每走一步，引导会在屏幕上高亮出脚本正操作的控件并停留一会儿（`--dwell` 调，
默认值见 DEFAULT_DWELL，设 0 关掉）。脚本本身是毫秒级的，不歇一下根本看不清——这是
**演示**才需要的节奏，真跑自动化时关掉。

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
        "找不到 funq 包。请用同目录的 funq-client-demo.bat / .sh 启动本脚本，\n"
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

# 动作是 QAction 不是 QWidget，但 funq 一样能按路径取（client.action）。
# 路径 = 它所属控件的路径 + 动作名，而本项目的动作都 new 在主窗口上
# （SARibbon 的 addLargeAction 不改父级），所以前缀就是 MainWindow 自己。
#
# 这行原先更长：MainWindow::QMenuBar::menuTool::actionSettings——主窗口换成
# 功能区（SARibbon）之后没有 QMenuBar 那一段了，路径随之变短。
# 容器名也一并变了：工具入口在功能区里是面板 ribbonPanelTool，不再是菜单 menuTool
SETTINGS_ACTION = "MainWindow::actionSettings"

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

# 左侧分类导航。设置对话框默认停在第一页（「通用」），要在别的页上操作得先
# 切过去——funq 的 client.widget() 默认等 visible，不在当前页的控件等不到
SETTINGS_CATEGORY_LIST = "MainWindow::settingsDialog::settingsCategoryList"
SETTINGS_ROW_LOG = 1  # 分类顺序：通用 / 日志 / 日志格式

# 每次高亮后的默认停留秒数。**只在这里定义一份**——类和命令行各写一个默认值时，
# 命令行那个恒定有值，会把类默认值悄悄盖掉（踩过）
DEFAULT_DWELL = 3.0

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


# 桥接对象是独立顶层窗口（不是主窗口的子控件），登录通过之后才创建——
# 引导演示的是主界面，登录框那几步不做高亮
BRIDGE = "tourBridge"


class Tour(object):
    """funq -> tour 的薄封装：脚本走到哪一步，屏幕上就高亮到哪一步。

    底层是 tourBridge 上的两个槽，见 docs/plan/tour库重构_决策记录.md 的
    「funq 桥接（对外契约）」。
    桥接**回报**失败（ok=false，含原因）时不中断脚本——它只是可视化，不该把
    用例拖垮。但桥接本身调不通（对象不在、连接断了）会抛异常，那是另一回事，
    会直接中断——那种情况挂掉比静默继续好查。

    `dwell` 是每次高亮后的停留秒数。funq 没有慢放开关（它只有"等条件"的
    wait_for_properties，挡不住高亮一闪而过），而脚本本身是毫秒级的——不歇
    一下，下一步立刻把上一步的高亮覆盖掉，人根本看不见。真正跑自动化时把它
    设成 0。
    """

    def __init__(self, client, dwell=DEFAULT_DWELL):
        self._client = client
        self._dwell = dwell
        self._bridge = None

    def _target(self):
        if self._bridge is None:
            # 登录通过之前桥接对象还不存在，到这里才去拿
            self._bridge = self._client.widget(path=BRIDGE, timeout=10.0)
        return self._bridge

    def highlight(self, path, title, text):
        result = self._target().call_slot(
            slot_name="showHighlight",
            params={"path": path, "title": title, "text": text})
        if result.get("ok"):
            # 把算出来的几何打出来：高亮框算在哪儿、遮罩铺在哪儿。这类
            # "看不见 / 位置不对"的问题，只有这几个数字能自查
            print("    [引导] 高亮框 %s | 遮罩 %s | 宿主 %s"
                  % (result.get("rect"), result.get("overlay"),
                     result.get("host")))
        else:
            print("    [引导] %s" % result.get("message"))
        # 停一会儿再往下走，让这一步在屏幕上看得到
        if self._dwell > 0:
            time.sleep(self._dwell)
        return result

    def clear(self):
        return self._target().call_slot(slot_name="hideHighlight", params={})


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--host", default="localhost",
                        help="libFunq 监听的地址（默认 localhost）")
    parser.add_argument("--port", type=int, default=9000,
                        help="libFunq 监听的端口（默认 9000）")
    parser.add_argument("--dwell", type=float, default=DEFAULT_DWELL,
                        help="每次高亮后停留的秒数（默认见 DEFAULT_DWELL；设 0 关掉，"
                             "想看清时调大）")
    opts = parser.parse_args()

    banner(1, "连上去")
    print("目标：%s:%d" % (opts.host, opts.port))
    print("服务端没起来的话，这里会等 10 秒然后报 ConnectionRefusedError。")
    try:
        client = FunqClient(host=opts.host, port=opts.port)
    except Exception as exc:
        sys.stderr.write("\n连不上（%s）。\n"
                         "确认另一个终端里的 funq-server-demo 还在跑。\n" % exc)
        return 1
    print("连上了。注意：整个过程 client 没启动过任何程序。")
    # 高亮助手：脚本每走一步，屏幕上就指到对应控件（失败不影响用例）
    print("每次高亮会停留 %.1f 秒（--dwell 可调），否则一步接一步全糊在一起。"
          % opts.dwell)
    guide = Tour(client, dwell=opts.dwell)

    # 登录这几步只在登录开着时成立：它可以在配置（login/enabled）或命令行
    # （--set）里关掉，那样根本没有登录对话框、主窗口启动即建。判据只能问
    # 界面——脚本是经 TCP 连过来的，看不到对方的配置与命令行。
    # 这几步不做高亮：引导演示的是主界面，桥接对象要登录之后才存在
    if exists(client, LOGIN_USERNAME):
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
    else:
        banner(2, "登录已关闭：跳过登录三步")
        print("目标程序的登录是关着的（配置 login/enabled=false，或命令行 --set）：")
        print("没有登录对话框，启动即建主窗口，所以「输错密码 → 看提示文字 →")
        print("登录后才建主窗口」这几条断言在这里无从谈起，直接往下走。")
        print("（判据是问界面有没有 %s，不读对方的配置与命令行。）"
              % LOGIN_USERNAME)

    # 两条路的汇合点：带登录时主窗口刚由登录按钮创建，不带时启动就有，都在
    # 这里等它出来
    main_window = client.widget(path=MAIN_WINDOW, timeout=10.0)
    print("主窗口已出现：%s" % brief(main_window, ("visible", "enabled")))
    print("中央占位控件的标题：%r"
          % client.widget(path=MAIN_TITLE).properties()["text"])

    banner(5, "从菜单触发「设置」")
    print("菜单动作是 QAction，用 client.action() 取，trigger() 触发。")
    print("坑在这里：trigger(blocking=True) 会一直等到动作做完，而「设置」")
    print("开的是模态对话框、自己不会返回——所以必须 blocking=False。")
    guide.highlight(MAIN_TITLE, "进主窗口了", "登录成功后主窗口才创建")
    # 这里刻意不高亮「工具」菜单：菜单项是 QAction 不是 QWidget，指不了；
    # 而菜单栏里的 QMenu 从没显示过、没有有效几何，指它会把遮罩画到屏幕角落
    action = client.action(path=SETTINGS_ACTION)
    print("拿到的动作：%r" % action.properties().get("text"))
    action.trigger(blocking=False)
    client.widget(path=SETTINGS_DIALOG, timeout=10.0)
    print("设置对话框出现了：%s" % SETTINGS_DIALOG)
    guide.highlight(SETTINGS_DIALOG, "设置对话框",
                    "它盖住了主窗口，高亮得跟过来才看得见")

    banner(6, "在设置对话框里读一个值")
    print("目标：%s" % SETTINGS_RETAIN_DAYS)
    print("（「日志」页的『保留天数』）")
    # 切到「日志」页：默认停在「通用」页，不在当前页的控件不可见，funq 等不到
    client.widget(path=SETTINGS_CATEGORY_LIST).set_property(
        "currentRow", SETTINGS_ROW_LOG)
    # 那个 QSpinBox 没有 objectName，路径里是合成段（settingsRow-1::QSpinBox），
    # 引导的解析器照 funq 的规则逐段下钻，认这个
    guide.highlight(SETTINGS_RETAIN_DAYS, "保留天数", "读一个值，再写回去")
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
    guide.clear()
    client.widget(path=SETTINGS_CLOSE).click()
    time.sleep(0.5)
    print("设置对话框还在不在：%s（应该不在）" % exists(client, SETTINGS_DIALOG))
    client.close()
    print("连接已断开。被测程序还在跑——它归终端 1 管，Ctrl+C 才结束它。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
