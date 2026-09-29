"""ctest 入口：跑本目录下的 funq 用例，按退出码给出结果。

用例通过 FunqTestCase 自己拉起被测程序（并让 funq 的 runner 把 libFunq 注入
进去），所以这里不需要额外的启动逻辑，只负责注册应用配置再跑 unittest。
"""

import os
import sys
import unittest


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    sys.path.insert(0, here)

    # Qt 的 bin 加进 PATH：被测程序由 funq 拉起，继承本进程的环境，找不到
    # Qt DLL 就起不来。CMake 的 ENVIRONMENT 是分号分隔的列表，而 PATH 本身
    # 就是分号分隔，写在那里会被拆散，所以由 CMake 传单个目录、这里自己拼
    qt_bin = os.environ.get("FUNQ_QT_BIN")
    if qt_bin:
        os.environ["PATH"] = qt_bin + os.pathsep + os.environ.get("PATH", "")

    import funq_bootstrap

    funq_bootstrap.prepare_sys_path()
    funq_bootstrap.register_for_tests()

    suite = unittest.TestLoader().discover(here, pattern="test_*.py")
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
