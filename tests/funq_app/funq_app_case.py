"""本仓 funq 用例的基类：起完应用先把登录过掉。

`APP_ENABLE_AUTH=ON` 的构建里，主窗口在登录成功之前**根本不存在**——不管
登录的话每条用例都会报"找不到 MainWindow"，而报错现场（路径找不到）离真正
的原因（还没登录）很远。

判据是**问界面有没有 `loginDialog`**，不读编译开关：夹具经 TCP 连过来，读
不到对方的编译开关（与 examples/funq-scripts 的演示脚本同一处置）。探一次
就够——"这个构建有没有登录框"是构建的属性，不会因为用例之间重起程序而变。

首登强制改密也在这儿过掉，按**原值**填：默认管理员带着 must_change 标记
（见 src/core/auth/user_manager.cpp 的 kDefaultAdmin），不替操作员定新密码。

**副作用要说清楚**：用户库落在 `QStandardPaths::AppDataLocation` 下
（`%LOCALAPPDATA%/<组织>/<程序名>/users.json`），产品侧没有把这个目录做成
可配的开关，所以这轮跑完会把开发机那份用户的 must_change 翻成 false。开发机
上想复原就删掉那个 users.json——下次运行会重新建出 admin/123。
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
    """有登录框就登进去；顺带过掉首登强制改密。没有登录框直接返回。"""
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
