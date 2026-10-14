"""启动 demo_app 并注入 libFunq，供同目录的 probe_*.py 连接。

用法:
    python scripts/funq/run_demo_app_funq.py [--build 构建目录] [--port 9000]
                                             [-- 被测程序参数...]

要点:
  - 构建目录默认 build/ninja-relwithdebinfo-x86（本仓的验证口径）
  - 解释器按被测程序的 PE 位数自动挑：x86 靶子必须用 32 位 python——
    64 位 runner 注不进 32 位进程，报的是误导性的
    "LoadLibraryW() returned 0 ... Maybe x86/x64 mismatch"
  - Qt 运行时路径（PATH/QT_PLUGIN_PATH）从该构建目录生成的
    run_demo_app.bat 里取，构建系统最清楚它们在哪
  - `--` 之后的参数原样传给被测程序，如 `-- --set login/animation=tv`

本窗口会一直占住（runner 陪被测程序到退出）；探针在另一个终端跑。
"""

import argparse
import os
import struct
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))


def exe_machine(path):
    """读 PE 头的 Machine：0x14c = x86，0x8664 = x64。"""
    with open(path, "rb") as fh:
        if fh.read(2) != b"MZ":
            return None
        fh.seek(0x3C)
        pe_off = struct.unpack("<I", fh.read(4))[0]
        fh.seek(pe_off)
        if fh.read(4) != b"PE\0\0":
            return None
        return struct.unpack("<H", fh.read(2))[0]


def embedded_python(machine):
    """按位数挑仓库自带的嵌入式解释器（它的 ._pth 已列好 funq 两个包）。"""
    name = "python-3.8.8-embed-win32" if machine == 0x14C else \
           "python-3.8.8-embed-amd64"
    path = os.path.join(REPO_ROOT, "3rdparty", name, "python.exe")
    return path if os.path.isfile(path) else sys.executable


def qt_env_from_run_script(build_dir):
    """从生成的 run_demo_app.bat 里抄 PATH 与 QT_PLUGIN_PATH。"""
    env = {}
    script = os.path.join(build_dir, "bin", "run_demo_app.bat")
    if not os.path.isfile(script):
        return env
    with open(script, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.strip()
            for key in ("PATH", "QT_PLUGIN_PATH"):
                head = 'set "%s=' % key
                if line.startswith(head):
                    value = line[len(head):].rstrip('"')
                    env[key] = value.replace(";%PATH%", "")
    return env


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build", default=os.path.join(
        REPO_ROOT, "build", "ninja-relwithdebinfo-x86"),
        help="构建目录（默认 build/ninja-relwithdebinfo-x86）")
    parser.add_argument("--port", type=int, default=9000)
    parser.add_argument("app_args", nargs="*",
                        help="传给被测程序的参数（放在 -- 之后）")
    args = parser.parse_args()

    exe = os.path.join(args.build, "bin", "demo_app.exe")
    injector = os.path.join(args.build, "bin", "Funq.dll")
    for path in (exe, injector):
        if not os.path.isfile(path):
            raise SystemExit("找不到 %s；先构建那个目录（或换 --build）" % path)

    machine = exe_machine(exe)
    python = embedded_python(machine)
    arch = {0x14C: "x86", 0x8664: "x64"}.get(machine, "未知")

    env = dict(os.environ)
    env.update(qt_env_from_run_script(args.build))
    env["PYTHONPATH"] = os.pathsep.join([
        os.path.join(REPO_ROOT, "3rdparty", "funq-master", "client"),
        os.path.join(REPO_ROOT, "3rdparty", "funq-master", "server"),
        env.get("PYTHONPATH", ""),
    ])

    cmd = [python, "-m", "funq_server.runner",
           "--port", str(args.port), "--library", injector, exe] + args.app_args
    print("被测程序 %s（%s），注入 %s" % (exe, arch, injector))
    print("解释器   %s" % python)
    print("探针连   localhost:%d" % args.port)
    if args.app_args:
        print("透传参数 %s" % " ".join(args.app_args))
    return subprocess.call(cmd, env=env)


if __name__ == "__main__":
    sys.exit(main())
