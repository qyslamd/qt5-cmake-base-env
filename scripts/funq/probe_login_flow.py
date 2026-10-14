"""登录窗端到端：空提交 → 错密码 → 正确凭据 → 主窗口，逐帧抓图。

用法:
    python scripts/funq/run_demo_app_funq.py -- --set login/enabled=true
    python scripts/funq/probe_login_flow.py <出图目录> [端口=9000]

判据是像素（错误横幅、主窗口功能区），不看属性——属性只能证明"设了"。
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import funq_common as fc  # noqa: E402  （自带路径前戏，必须后置）

FORM = "loginDialog::loginRightPanel::loginFormHost"
USERNAME = FORM + "::loginUsername"
PASSWORD = FORM + "::loginFieldWrap::loginPassword"
SUBMIT = FORM + "::loginSubmit"
LOGIN = "loginDialog"


def main():
    out_dir, port = sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 9000
    c = fc.connect(port)

    tops = [p for p in fc.flatten(c.widgets_list()) if "::" not in p]
    print("顶层对象: %s" % tops)
    if LOGIN not in tops:
        raise SystemExit("登录窗不在：别带 --set login/enabled=false 起程序")
    fc.grab(c, LOGIN, os.path.join(out_dir, "01_login_window.png"))

    # 空提交。配置会预填凭据，先清空才是"空"分支（否则走真实鉴权）
    c.widget(path=USERNAME).set_property("text", "")
    c.widget(path=PASSWORD).set_property("text", "")
    time.sleep(0.3)
    c.widget(path=SUBMIT).click()
    time.sleep(0.6)
    fc.grab(c, LOGIN, os.path.join(out_dir, "02_empty_submit.png"))

    # 错密码：横幅应显示 AuthService 的真实原因，窗口不关
    c.widget(path=USERNAME).set_property("text", "admin")
    c.widget(path=PASSWORD).set_property("text", "wrongpass")
    time.sleep(0.3)
    c.widget(path=SUBMIT).click()
    time.sleep(0.8)
    fc.grab(c, LOGIN, os.path.join(out_dir, "03_bad_password.png"))

    # 正确凭据：窗口 accept，主窗口建起
    c.widget(path=USERNAME).set_property("text", "admin")
    c.widget(path=PASSWORD).set_property("text", "123")
    time.sleep(0.3)
    c.widget(path=SUBMIT).click()
    time.sleep(2.0)
    tops = [p for p in fc.flatten(c.widgets_list()) if "::" not in p]
    print("提交后顶层对象: %s" % tops)
    if "MainWindow" in tops:
        fc.grab(c, "MainWindow", os.path.join(out_dir, "04_main_window.png"))
    else:
        raise SystemExit("没进主窗口——登录分支有问题，看 03 那张图")


if __name__ == "__main__":
    main()
