"""本仓 funq 用例的基类。

正常路径下用例不需要关心登录：funq.conf 的 args 带
`--set login/enabled=false`，登录框不出现、MainWindow 启动即建，也免去了
首登强制改密对 users.json 的副作用。

ensure_logged_in 在这儿是**兜底**：有人把 funq.conf 里那条 --set 删掉、想
演练登录流程本身时，它探到 loginDialog 仍会照常登进去（admin/123），并按
原值过掉首登强制改密。判据是**问界面有没有 loginDialog**，不读对方任何
配置或命令行——夹具经 TCP 连过来，读不到。
"""

import time

from funq.testcase import FunqTestCase

# 登录框的控件路径。**中间那两级不能省**：funq 的 objectpath 把祖先链上每
# 一级都拼进来（登录框是 loginDialog::loginContent::loginForm 里的）
LOGIN_DIALOG = "loginDialog"
LOGIN_FORM = LOGIN_DIALOG + "::loginContent::loginForm"
LOGIN_USERNAME = LOGIN_FORM + "::loginUsername"
LOGIN_PASSWORD = LOGIN_FORM + "::loginPassword"
LOGIN_BUTTON = LOGIN_FORM + "::loginButton"
CHANGE_PASSWORD_DIALOG = "changePasswordDialog"
CHANGE_PASSWORD_OLD = CHANGE_PASSWORD_DIALOG + "::changePasswordOld"
CHANGE_PASSWORD_NEW = CHANGE_PASSWORD_DIALOG + "::changePasswordNew"
CHANGE_PASSWORD_CONFIRM = CHANGE_PASSWORD_DIALOG + "::changePasswordConfirm"
CHANGE_PASSWORD_OK = CHANGE_PASSWORD_DIALOG + "::changePasswordOk"

# 首次运行自动创建的管理员账号（user_manager.cpp 的 kDefaultAdmin / kDefaultPassword）
GOOD_USER, GOOD_PASSWORD = "admin", "123"

# None = 还没探过。见模块 docstring
_has_login = None

# 首登改密一轮里**只可能遇到一次**（标志落盘就翻了）：探过一次就不再探。
# 不这么收口的话，后面每条用例都要拿 3 秒去等一个不会出现的对话框——一轮
# 几十条就是白等一两分钟
_first_login_checked = False


def exists(client, path, timeout=1.0):
    """路径在不在（不等待可见）。

    **路径在 ≠ 窗口开着**：服务端的查找只沿 QObject 的父子链对 objectName，
    不看 visible（objectpath.cpp 的 findObject），所以一个只是被隐藏的窗口
    照样找得到——日志窗口就是"建一次、反复显隐"的。要断言"某个窗口关掉了"，
    读它的 visible 属性（或 wait_for_properties），别用这个。
    """
    try:
        client.widget(path=path, timeout=timeout, wait_active=False)
        return True
    except Exception:
        return False


def _fill(client, path, text):
    client.widget(path=path).set_property("text", text)


def ensure_logged_in(client):
    """有登录框就登进去；顺带过掉首登强制改密。没有登录框直接返回。

    正常路径（funq.conf 带 --set login/enabled=false）下登录框不存在，
    这里一步就返回；探一次就够——登录开不开是启动配置的属性，不会因为
    用例之间重起程序而变。
    """
    global _has_login, _first_login_checked
    if _has_login is None:
        _has_login = exists(client, LOGIN_DIALOG, timeout=3.0)
    if not _has_login:
        return

    _fill(client, LOGIN_USERNAME, GOOD_USER)
    _fill(client, LOGIN_PASSWORD, GOOD_PASSWORD)
    client.widget(path=LOGIN_BUTTON).click()

    # 只第一次等它（见 _first_login_checked 那段）。等不到就是"这台机器的用户库
    # 已经过了首登"，正是常态
    if not _first_login_checked:
        if exists(client, CHANGE_PASSWORD_DIALOG, timeout=3.0):
            _fill(client, CHANGE_PASSWORD_OLD, GOOD_PASSWORD)
            _fill(client, CHANGE_PASSWORD_NEW, GOOD_PASSWORD)
            _fill(client, CHANGE_PASSWORD_CONFIRM, GOOD_PASSWORD)
            client.widget(path=CHANGE_PASSWORD_OK).click()
            # 改密框关掉之后主窗口才建——下面那句 wait 同时是在等它真的关掉
            deadline = time.time() + 10.0
            while exists(client, CHANGE_PASSWORD_DIALOG, timeout=0.2) \
                    and time.time() < deadline:
                time.sleep(0.1)
        _first_login_checked = True

    # 主窗口出现 = 登录真的过了。**别省这句**：后面每条用例都以它为前提，
    # 在这儿等，失败现场才落在"登录"上
    client.widget(path="MainWindow", timeout=30.0)


class DemoAppFunqTestCase(FunqTestCase):
    """本仓 funq 用例的基类：`super().setUp()` 起完程序就先过登录。

    页面/对话框自己的基类从它派生。**子类覆写 setUp 时记得调 super()**——
    这条链上每一环都在做必要的准备。
    """

    __app_config_name__ = "demo_app"

    def setUp(self):
        super().setUp()
        ensure_logged_in(self.funq)
