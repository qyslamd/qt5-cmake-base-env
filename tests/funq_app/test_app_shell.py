"""真程序的外壳用例：连得上、登录过得去、界面按预期长出来。

这一组刻意只碰外壳——主窗口、占位页、动作、入口形态——不碰业务。夹具的意义
是**跑得起来**：它红了说明夹具或环境坏了，而不是某条业务逻辑错了。

控件路径都不写在用例里，走同目录的 demo_app.alias（funq 按别名在本地查表，
不占一次往返）。
"""

import os
import time

from funq_app_case import DemoAppFunqTestCase, exists

# 两条入口形态的 objectName。**不拼完整路径**：中间那几级是 Qt 与 SARibbon
# 的内部容器名（见 demo_app.alias 里的说明），拼死了等于把用例绑到第三方实现
RIBBON_ENTRIES = ("ribbonHomeCategory", "ribbonPanelFile", "ribbonPanelTool",
                  "ribbonPanelHelp")
MENU_ENTRIES = ("menuFile", "menuTool", "menuHelp")


def find_names(tree, wanted):
    """在 widgets_list() 的树里按 objectName 找，返回命中的那一组名字。"""
    found = set()
    stack = [tree]
    while stack:
        for name, node in stack.pop().items():
            if name in wanted:
                found.add(name)
            stack.append(node.get("children") or {})
    return found


class AppShellTest(DemoAppFunqTestCase):
    def test_main_window_is_visible(self):
        """注入成功、登录已过、主窗口就在那儿。

        登录之后才有主窗口，所以这条同时也是"ensure_logged_in 真的过掉了"
        的判据——基类里那 30 秒的等待若超时，红在这里。
        """
        props = self.funq.widget(alias="window").properties()
        self.assertTrue(props["visible"])
        self.assertTrue(props["enabled"])

    def test_placeholder_page_shows_title_and_hint(self):
        """占位页的两行字都在。

        只断言"有字"，不断言字是什么：这页就是留给模板使用者替换的，改文案
        不该让用例红。
        """
        for alias in ("page_title", "page_hint"):
            props = self.funq.widget(alias=alias).properties()
            self.assertTrue(props["visible"], alias)
            self.assertTrue(props["text"].strip(), alias)

    def test_actions_are_addressable(self):
        """五个动作都带 objectName。

        名字是引导与 funq 共用的锚点（见 mainwindow.cpp 的 createActions）：
        没有它 funq 只能拿"类名-序号"定位，那种名字改一次界面就变。
        """
        for alias in ("action_quit", "action_log_window",
                      "action_settings", "action_make_crash",
                      "action_about"):
            action = self.funq.action(alias=alias)
            self.assertTrue(action.properties()["enabled"], alias)

    def test_log_action_opens_log_window(self):
        """动作真的接在界面上：触发它，窗口出现；点关闭，窗口藏起来。

        「关得掉」这半条不能省——被脚本打开的窗口若没有应用内的关闭入口，它
        在脚本里就是"只进不出"（见 log_window.cpp 里那段注释）。

        判据是 visible 而不是"路径还在不在"：日志窗口是建一次反复显隐的，
        关掉之后对象仍在（见 funq_app_case.exists 的说明）。
        """
        # 模态对话框会卡住 trigger，所以一律不阻塞地触发
        self.funq.action(alias="action_log_window").trigger(blocking=False)
        window = self.funq.widget(alias="log_window", timeout=10.0)
        self.assertTrue(window.properties()["visible"])

        self.funq.widget(alias="log_close").click()
        window.wait_for_properties({"visible": False}, timeout=5.0)
        self.assertFalse(window.properties()["visible"])

    def test_settings_dialog_shows_restart_banner(self):
        """设置对话框顶部有「需重启生效」横幅。

        登录、崩溃捕获后端、日志文件这些只在启动时读一次，这件事要讲在
        界面明处。断言横幅可见即可——文案措辞允许改，改了不该红用例。
        """
        # 模态对话框会卡住 trigger，所以一律不阻塞地触发
        self.funq.action(alias="action_settings").trigger(blocking=False)
        dialog = self.funq.widget(alias="settings_dialog", timeout=10.0)
        self.assertTrue(dialog.properties()["visible"])

        banner = self.funq.widget(
            path="MainWindow::settingsDialog::settingsRestartBanner")
        self.assertTrue(banner.properties()["visible"])

        self.funq.widget(alias="settings_close").click()
        # 设置对话框是栈上的模态对象，关掉即析构——再问它的属性会报
        # NotRegisteredObject（与日志窗口"建一次反复显隐"不同），所以"关掉
        # 与否"要用路径在不在来判断
        deadline = time.time() + 5.0
        while exists(self.funq, "MainWindow::settingsDialog", timeout=0.2) \
                and time.time() < deadline:
            time.sleep(0.1)
        self.assertFalse(
            exists(self.funq, "MainWindow::settingsDialog"),
            "设置对话框关掉之后不该再找得到")

    def test_about_dialog_shows_current_command_line(self):
        """「关于」里显示的是**本次运行**的命令行，且可选中复制。

        断言两条：可执行文件名在（argv[0] 一定在命令行里），以及夹具经
        funq.conf 传进来的 --set 原样出现——后者正是"显示实参而非参数清单"
        的判据。可选中 = 能复制去终端复现。
        """
        # 模态对话框会卡住 trigger，所以一律不阻塞地触发
        self.funq.action(alias="action_about").trigger(blocking=False)
        self.funq.widget(alias="about_dialog", timeout=10.0)

        props = self.funq.widget(alias="about_command_line").properties()
        text = props["text"]
        self.assertIn("demo_app", text, "命令行里没有可执行文件名：%r" % text)
        self.assertIn("--set crash/backend=none", text,
                      "命令行里没有夹具传的实参：%r" % text)
        # 可选中 = 能复制去终端用；枚举值见 Qt::TextInteractionFlag
        self.assertTrue(props["textInteractionFlags"] & 0x1,
                        "命令行文本不可选中：%r" % props["textInteractionFlags"])
        self.funq.widget(path="MainWindow::aboutDialog::QDialogButtonBox"
                              "::QPushButton").click()

    def test_entry_points_match_the_build(self):
        """入口形态恰好是一种，且和构建配置对得上。

        功能区与菜单栏是编译期二选一（BUILD_RIBBON）。这条用例两头都认，
        同时验了 CMake 有没有把 FUNQ_RIBBON 正确传进来——传错了会让别的用
        例静默地走错分支。
        """
        tree = self.funq.widgets_list()
        ribbon = find_names(tree, RIBBON_ENTRIES)
        menus = find_names(tree, MENU_ENTRIES)
        self.assertNotEqual(
            bool(ribbon), bool(menus),
            "应当恰好是一种入口形态，实际功能区=%s 菜单栏=%s"
            % (sorted(ribbon), sorted(menus)))
        self.assertEqual(sorted(ribbon or menus),
                         sorted(RIBBON_ENTRIES if ribbon else MENU_ENTRIES))

        expected = os.environ.get("FUNQ_RIBBON")
        if expected is not None:
            self.assertEqual(bool(ribbon), expected == "1",
                             "FUNQ_RIBBON 与实际界面不符")
