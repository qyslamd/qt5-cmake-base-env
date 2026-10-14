"""密码眼睛按钮的两态：未勾选（斜杠）/ 勾选（睁眼），顺带读 checked 属性。

用法:
    python scripts/funq/run_demo_app_funq.py
    python scripts/funq/probe_eye_button.py <出图目录> [端口=9000]
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import funq_common as fc  # noqa: E402

EYE = ("loginDialog::loginRightPanel::loginFormHost::loginFieldWrap"
       "::loginTogglePwd")


def main():
    out_dir, port = sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 9000
    c = fc.connect(port)

    for tag in ("eye_unchecked", "eye_checked"):
        if tag == "eye_checked":
            c.widget(path=EYE).click()
            time.sleep(0.3)
        fc.grab(c, EYE, os.path.join(out_dir, tag + ".png"))
        props = c.widget(path=EYE).properties()
        print("saved %s.png  checked=%s" % (tag, props.get("checked")))

    c.widget(path=EYE).click()  # 收尾回 off
    time.sleep(0.2)


if __name__ == "__main__":
    main()
