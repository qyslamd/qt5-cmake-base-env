"""文档预览的像素验证：抓 MarkdownView 存 PNG，供人（或模型）读图。

属性只能证明"设了"，像素才能证明"渲染出来了"——markdown 的排版、代码块着色、
中文有没有变方框，都只有像素说了算。

两终端用法（终端 1 起程序，终端 2 抓图）：

    rem 终端 1
    3rdparty\\python-3.8.8-embed-win32\\python.exe scripts\\funq\\run_demo_app_funq.py ^
        -- --set login/enabled=false --doc docs/guide/模板使用指南.md

    rem 终端 2
    3rdparty\\python-3.8.8-embed-win32\\python.exe scripts\\funq\\probe_markdown_view.py ^
        temp\\shots\\md

两个前置条件别忘：登录窗会拦在 --doc 前面（故要 --set login/enabled=false）；
已有实例在跑时新进程会把参数转发过去就退出，抓不到东西。

x86 靶子必须用 3rdparty\\python-3.8.8-embed-win32 那份 python 当 runner。
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import funq_common


def main():
    out_png = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        "temp", "shots", "markdown_view.png")
    os.makedirs(os.path.dirname(out_png) or ".", exist_ok=True)

    client = funq_common.connect()

    # 顶层那段用 objectName（markdownWindow），不是类名——类名兜底会在改类名时
    # 悄悄断掉探针
    paths = funq_common.flatten(client.widgets_list())
    print("控件树里与文档预览相关的：")
    for path in paths:
        if "markdown" in path.lower():
            print("  " + path)

    funq_common.grab(client, "markdownWindow::markdownView", out_png)
    print("已存 %s（%d 字节）" % (out_png, os.path.getsize(out_png)))


if __name__ == "__main__":
    main()
