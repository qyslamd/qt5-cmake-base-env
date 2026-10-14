"""登录窗左栏动画：隔 1s 连抓 3 帧（证明"在动"）+ 开头列一次顶层对象。

用法:
    python scripts/funq/run_demo_app_funq.py -- --set login/animation=tv
    python scripts/funq/probe_login_anim.py <出图目录> [标签] [端口=9000]

为什么连抓 3 张：电视的 10s 循环与火箭的 18s 循环里都有静止段，两张可能
撞同一相位；三张才能证"在动"。判据是像素——动画状态没有可读的属性载体。
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import funq_common as fc  # noqa: E402

PANEL = "loginDialog::loginLeftPanel"


def main():
    out_dir = sys.argv[1]
    tag = sys.argv[2] if len(sys.argv) > 2 else "anim"
    port = int(sys.argv[3]) if len(sys.argv) > 3 else 9000

    c = fc.connect(port)
    print("顶层对象: %s" % [p for p in fc.flatten(c.widgets_list())
                        if "::" not in p])

    for i in range(3):
        path = os.path.join(out_dir, "%s_%d.png" % (tag, i))
        fc.grab(c, PANEL, path)
        print("saved %s" % path)
        time.sleep(1.0)


if __name__ == "__main__":
    main()
