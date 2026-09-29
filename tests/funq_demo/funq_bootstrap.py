"""把 funq 接到本仓的测试环境里（不依赖 nose）。

funq 原本由它的 nose 插件负责两件事：解析 funq.conf、把应用配置注册给测试
基类。nose 在 Python >= 3.10 上已无法运行（依赖被移除的 imp 模块），所以这里
照插件的行为给出等价物——注册逻辑本身用的是 funq 自带的公开接口：

    ApplicationRegistry.register_from_conf(configparser, global_options)
    funq.testcase.register_funq_app_registry(registry)

注册之后，继承自 FunqTestCase 的用例就能用 __app_config_name__ 指定
funq.conf 里的段名，测试框架会自己拉起被测程序。

路径全部由环境变量给出（CMake 在 add_test 里注入，见 CMakeLists.txt）：
    FUNQ_CONF        funq.conf 路径
    FUNQ_ATTACH_EXE  注入用可执行文件（funq 命令行包装，见 funq_attach.bat.in）
    FUNQ_CLIENT_DIR  funq 客户端包所在目录（加入 sys.path）
    FUNQ_SERVER_DIR  funq 服务端包所在目录（加入 sys.path）
"""

import configparser
import os
import sys


class GlobalOptions(object):
    """funq 原本由 nose 插件提供的全局选项对象。

    属性名照抄插件的选项定义（client/funq/noseplugin.py 的 add_option），
    少一个就会在 ApplicationConfig.from_conf 里抛 AttributeError。
    """

    def __init__(self, conf_path, attach_exe):
        self.funq_conf = conf_path
        self.funq_attach_exe = attach_exe
        self.funq_gkit = "default"
        self.funq_gkit_file = None
        self.funq_trace_tests = None
        self.funq_trace_tests_encoding = None
        self.funq_screenshot_folder = None
        self.funq_snooze_factor = None


def prepare_sys_path():
    """把 funq 的两个包目录加进 sys.path。"""
    for key in ("FUNQ_CLIENT_DIR", "FUNQ_SERVER_DIR"):
        directory = os.environ.get(key)
        if not directory:
            raise RuntimeError("环境变量未设置：%s" % key)
        if directory not in sys.path:
            sys.path.insert(0, directory)


def load_conf():
    """读 funq.conf，返回 (ConfigParser, 配置文件绝对路径)。"""
    conf_path = os.environ.get("FUNQ_CONF")
    if not conf_path or not os.path.isfile(conf_path):
        raise RuntimeError("环境变量 FUNQ_CONF 未指向存在的文件：%r" % conf_path)
    parser = configparser.ConfigParser()
    parser.read(conf_path, encoding="utf-8")
    return parser, conf_path


def make_registry(parser, conf_path):
    """构造并注册应用配置，返回 registry。"""
    from funq.client import ApplicationRegistry

    attach_exe = os.environ.get("FUNQ_ATTACH_EXE")
    if not attach_exe:
        raise RuntimeError("环境变量未设置：FUNQ_ATTACH_EXE")

    registry = ApplicationRegistry()
    registry.register_from_conf(parser, GlobalOptions(conf_path, attach_exe))
    return registry


def register_for_tests():
    """供用例模块调用：注册配置，之后 FunqTestCase 即可正常工作。"""
    from funq import testcase as funq_testcase

    parser, conf_path = load_conf()
    funq_testcase.register_funq_app_registry(make_registry(parser, conf_path))
