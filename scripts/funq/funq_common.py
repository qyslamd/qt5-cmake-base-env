"""funq 探针的公共前戏：把 funq 的 client/server 挂进 sys.path。

放在同目录供各 probe_*.py 导入。仓库自带的嵌入式 python 已在 ._pth 里
列了这两个目录，这里再兜一道——用系统 python 跑时也成立。
"""

import os
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))

for _sub in ("client", "server"):
    _dir = os.path.join(REPO_ROOT, "3rdparty", "funq-master", _sub)
    if _dir not in sys.path:
        sys.path.insert(0, _dir)


def connect(port=9000):
    """连上正在跑的被测程序（由 run_demo_app_funq.py 或 funq-server-*.bat 起）。"""
    from funq.client import FunqClient

    return FunqClient(host="localhost", port=port)


def grab(client, path, out_png):
    """按 oid 抓单个控件存成 PNG（窗口被压着也拍得到）。"""
    import base64

    w = client.widget(path=path, timeout=20.0)
    data = client.send_command("grab", oid=w.oid, format="PNG")
    with open(out_png, "wb") as fh:
        fh.write(base64.standard_b64decode(data["data"]))
    return out_png


def flatten(tree, prefix=""):
    """widgets_list() 的嵌套树摊成路径列表。"""
    paths = []
    for name, node in tree.items():
        path = "%s::%s" % (prefix, name) if prefix else name
        paths.append(path)
        paths.extend(flatten(node.get("children", {}), path))
    return paths
