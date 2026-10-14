"""电视动画的交互验证（设计文档 docs/plan/电视动画交互化设计.md §五 的协议）。

用法:
    python scripts/funq/run_demo_app_funq.py -- --set login/animation=tv
    python scripts/funq/probe_tv_interaction.py <出图目录> [端口=9000]

三条协议要点（都踩过）：
  - 归一化穿梭：焦点依次给 用户名 → 密码 → 用户名。typing 只由
    editingFinished（回车或失焦）清掉，且 funq 的 keyclick 不改焦点——
    不穿梭的话后面每条断言都落在"打字档"上
  - 偷看要用 set_property 先让密码非空（门控），且点击前先抓一帧：循环
    自己的锁定平台上画面本就接近满卡，不比会被误判
  - 打字只能靠 keyclick（带 oid）：set_property 走 setText，只发 textChanged
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import funq_common as fc  # noqa: E402

PANEL = "loginDialog::loginLeftPanel"
FORM = "loginDialog::loginRightPanel::loginFormHost"
USERNAME = FORM + "::loginUsername"
WRAP = FORM + "::loginFieldWrap"
PASSWORD = WRAP + "::loginPassword"
EYE = WRAP + "::loginTogglePwd"
SUBMIT = FORM + "::loginSubmit"


def main():
    out_dir, port = sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 9000
    c = fc.connect(port)

    def shot(tag):
        fc.grab(c, PANEL, os.path.join(out_dir, tag + ".png"))
        print("saved %s.png" % tag)

    def focus(path):
        c.widget(path=path).activate_focus()
        time.sleep(0.08)

    def normalize():
        for path in (USERNAME, PASSWORD, USERNAME):
            focus(path)
        time.sleep(0.2)

    def text(path, value):
        c.widget(path=path).set_property("text", value)

    normalize()
    shot("00_baseline_a")
    time.sleep(0.6)
    shot("00_baseline_b")

    # 聚焦密码框：轻微失锁 + 慢跑（两帧间隔 600ms 才看得出行位移动）
    focus(PASSWORD)
    shot("01_focus_a")
    time.sleep(0.6)
    shot("01_focus_b")
    normalize()

    # 偷看：密码非空（门控）→ 点眼睛 → 满卡；+1.5s 回雪
    text(PASSWORD, "abc")
    time.sleep(0.2)
    shot("02_pre_eye")
    c.widget(path=EYE).click()
    time.sleep(0.3)
    shot("02_peek")
    time.sleep(1.5)
    shot("02_after_peek")

    # 门控：眼睛先置回 off（否则这点是"关"动作、测不到边沿），密码清空
    c.widget(path=EYE).click()
    time.sleep(0.3)
    text(PASSWORD, "")
    time.sleep(0.2)
    c.widget(path=EYE).click()
    time.sleep(0.3)
    shot("03_gate_empty")
    c.widget(path=EYE).click()
    time.sleep(0.2)

    # 打字：keyclick 必须带 oid（客户端级会打到 activeWindow，静默无效）
    normalize()
    c.widget(path=PASSWORD).keyclick("a")
    time.sleep(0.25)
    shot("04_typing_a")
    time.sleep(0.6)
    shot("04_typing_b")

    # 错误：清空两个框（配置会预填，不然走真实鉴权失败分支）
    text(USERNAME, "")
    text(PASSWORD, "")
    time.sleep(0.2)
    c.widget(path=SUBMIT).click()
    time.sleep(0.3)
    shot("05_error_a")
    time.sleep(1.2)
    shot("05_error_b")
    time.sleep(1.6)
    normalize()
    shot("06_after_error")


if __name__ == "__main__":
    main()
