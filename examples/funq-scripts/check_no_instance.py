"""检查被测程序是否已经在运行——起进程之前先问一句。

**为什么需要它**：funq 的 runner 是「先起进程、再注入」。而被测程序自己有
单实例检测，已有实例时新起的进程会转发参数后立刻退出：

    Windows  runner 睡 1 秒后 poll() 发现进程没了，抛
             RuntimeError: The process has finished with an error ...
    Linux    runner 连存活检查都没有，静默地"成功"返回

两种都不说真实原因（已经有实例在跑），会把人往"注入失败/DLL 不匹配"上带。
这个脚本在起进程之前就把话说清楚。

**判据复用被测程序自己的单实例端点**，不按进程名扫。`SingleInstance` 用
`QLocalServer` 占住一个以 `applicationName` 命名的端点：

    Windows  \\\\.\\pipe\\<name>          命名管道
    Linux    <临时目录>/<name>            unix domain socket

按进程名扫只能回答"有没有叫这名字的进程"，而这里要问的是"这个应用占没占
位"；而且崩溃残留的 socket 文件按名字看还在，按"能不能连上"看就已经没了。

**它只是"提前告知"，不是唯一防线。** 探测与启动之间有竞态，被测程序自己的
检测仍然是权威——所以任何探测失败（连不上、路径不对、平台不认识）都按"没有
实例"放行，绝不拦。宁可漏报，不可误拦。
"""

import argparse
import os
import socket
import sys
import tempfile

# Windows 下 stderr 不是控制台时（重定向、管道、ctest），Python 按系统区域
# 编码输出（中文机器上是 GBK），而启动器已经把控制台切到 UTF-8、日志与 CI
# 也都按 UTF-8 看——显式钉死 UTF-8，免得同一句话在窗口里正常、重定向后变乱码
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8")


def _endpoint(name):
    """被测程序占住的那个端点。和 SingleInstance 的命名规则保持一致。"""
    if sys.platform == "win32":
        return r"\\.\pipe\%s" % name
    # Qt 的 QLocalServer 在 Unix 侧落在 QDir::tempPath()，也就是 $TMPDIR（
    # 没设则 /tmp）。这里用 tempfile.gettempdir() 对齐同样的规则——**探测方
    # 与被探测方必须在同一个 TMPDIR 下**，否则会探不到。真的不一致时按下面
    # 的约定当"没有实例"放行，不会拦住启动
    return os.path.join(tempfile.gettempdir(), name)


def is_running(name):
    """连一下那个端点。连得上才算有实例。"""
    endpoint = _endpoint(name)
    if sys.platform == "win32":
        try:
            with open(endpoint, "r+b"):
                return True
        except OSError:
            return False

    probe = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        probe.connect(endpoint)
        return True
    except OSError:
        return False
    finally:
        probe.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--name", required=True,
                        help="被测程序的 applicationName（端口名就是它）")
    opts = parser.parse_args()

    if not is_running(opts.name):
        return 0

    sys.stderr.write(
        "已有实例在运行：%s\n"
        "程序本体只允许一个实例，所以不会再起一个去注入——那样 runner 只会\n"
        "报一句看不出原因的错。两条路：\n"
        "  1. 关掉正在运行的那个，再跑本脚本所在的启动器；\n"
        "  2. 如果它已经被注入过，直接用 funq 客户端连上它。\n"
        % _endpoint(opts.name))
    return 1


if __name__ == "__main__":
    sys.exit(main())
