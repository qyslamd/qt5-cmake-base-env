"""控件路径探针：启动被测程序，打印它的控件路径（可按关键字过滤）。

背景：funq 的路径优先取 objectName（见 3rdparty/funq-master/server/libFunq/
objectpath.cpp 的 rawObjectName），取不到才退化成「类名 + 同名兄弟的序号后缀」
——后者取决于子对象的创建顺序，还可能被布局/分裂器重新挂载父对象，只能从
运行中的进程里查。真程序 demo_app 的控件大多带 objectName，所以正常情况下
路径可以直接按「窗口::控件」拼出来；这个探针是给两种场合兜底的：核对路径、
或将来往界面里加了没设 objectName 的控件。

与 tests/funq_demo 那份同名脚本的差别只有两处，都是靶子不同带来的：配置段名
是 demo_app；以及**连上之后先登录**（带登录的构建里主窗口在登录成功前根本
不存在，不登录的话探针只能打印出登录框那几个控件）。

用的是 funq 客户端自带的接口：
  * FunqClient.widgets_list() 返回嵌套树，子控件在 "children" 键下
    （服务端 recursive_list_widget 的实现如此），不是扁平字典；
  * FunqClient.widget(path=...) 按路径取单个控件及其属性。

用法：
    set FUNQ_CONF=<构建目录>/tests/funq_app/funq.conf
    python probe_widgets.py [关键字]        # 列出路径含该关键字的控件
    python probe_widgets.py --props 路径... # 查指定路径的属性
"""

import configparser
import os
import sys

# funq 的两个包目录由 CMake 注入到环境变量（见 tests/funq_app/CMakeLists.txt）；
# 本目录自己也要进 path，下面要 import funq_app_case
_dir_here = os.path.dirname(os.path.abspath(__file__))
for _dir in (os.environ.get("FUNQ_CLIENT_DIR"),
             os.environ.get("FUNQ_SERVER_DIR"), _dir_here):
    if _dir and _dir not in sys.path:
        sys.path.insert(0, _dir)

from funq.client import ApplicationContext, ApplicationRegistry

from funq_app_case import ensure_logged_in

# 打印属性时只看这几项，够用来写断言
INTERESTING = ("visible", "enabled", "text", "value", "count",
               "currentIndex", "checked", "active")


def flatten(tree, prefix=""):
    """把 widgets_list() 的嵌套树摊成路径列表。"""
    paths = []
    for name, node in tree.items():
        path = "%s::%s" % (prefix, name) if prefix else name
        paths.append(path)
        paths.extend(flatten(node.get("children", {}), path))
    return paths


def main():
    conf = os.environ.get("FUNQ_CONF")
    if not conf or not os.path.isfile(conf):
        raise SystemExit("请先设置 FUNQ_CONF 指向 funq.conf：%r" % conf)

    registry = ApplicationRegistry()
    parser = configparser.ConfigParser()
    parser.read(conf, encoding="utf-8")
    registry.register_from_conf(parser, _GlobalOptions(conf))
    config = registry.config("demo_app")

    ctx = ApplicationContext(config)
    try:
        ensure_logged_in(ctx.funq)
        args = sys.argv[1:]
        if args and args[0] == "--props":
            for path in args[1:]:
                try:
                    props = ctx.funq.widget(path=path).properties()
                    brief = {k: props[k] for k in INTERESTING if k in props}
                    print("%s\n    %s" % (path, brief))
                except Exception as exc:  # noqa: BLE001 - 探针就是要显示失败原因
                    print("%s\n    查询失败: %s" % (path, exc))
            return

        keyword = args[0] if args else ""
        paths = [p for p in flatten(ctx.funq.widgets_list()) if keyword.lower()
                 in p.lower()]
        for path in paths:
            print(path)
        print("\n共 %d 个路径（过滤词: %r）" % (len(paths), keyword))
    finally:
        ctx.terminate()


class _GlobalOptions(object):
    """funq 原本由 nose 插件提供的全局选项对象，这里手工给出等价物。"""

    def __init__(self, conf_path):
        self.funq_conf = conf_path
        self.funq_attach_exe = os.environ["FUNQ_ATTACH_EXE"]
        self.funq_gkit = "default"
        self.funq_gkit_file = None
        self.funq_trace_tests = None
        self.funq_trace_tests_encoding = None
        self.funq_screenshot_folder = None
        self.funq_snooze_factor = None


if __name__ == "__main__":
    main()
