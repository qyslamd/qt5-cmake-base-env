"""验证控件的 hover 效果：挪真实光标上去，抓图对比前后两帧（仅 Windows）。

用法:
    python scripts/funq/run_demo_app_funq.py
    python scripts/funq/probe_hover.py <控件路径> <出图目录> [端口=9000] [进程名=demo_app]

为什么这么绕：funq 没有鼠标移动命令（客户端只有 click），而 hover 是窗口
系统事件——只能挪真实光标。两个坑（都踩过）：

  1. **目标窗口必须置顶**。被别的窗口（比如终端）压着时，光标挪过去了，
     事件也到不了被测程序。用 SetWindowPos(HWND_TOPMOST) 提到最前，用完
     还原 NOTOPMOST。
  2. **坐标要算准**。Qt 顶层窗口的 x()/y() 给的是"帧原点"，而子控件坐标
     是相对**客户区**的，中间差一个标题栏的高度（实测 39px，够让光标落到
     隔壁控件上）。所以：用 ClientToScreen 拿客户区原点，再逐级叠加子控件
     的 x/y，最后取控件中心。

跑完打印两帧 md5：不同即 hover 生效（再自己看两张图确认形态）。
"""

import hashlib
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import funq_common as fc  # noqa: E402

PS_PRELUDE = """
Add-Type @'
using System;
using System.Runtime.InteropServices;
public class Q {
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT p);
}
'@
Add-Type -AssemblyName System.Windows.Forms
"""

HWND_TOPMOST = -1
HWND_NOTOPMOST = -2
SWP_NOMOVE_NOSIZE = 0x3


def powershell(body):
    out = subprocess.run(["powershell", "-NoProfile", "-Command",
                          PS_PRELUDE + body],
                         capture_output=True, text=True, check=True)
    return out.stdout.strip()


def client_origin(proc):
    out = powershell("""
$h = (Get-Process %s).MainWindowHandle
$pt = New-Object Q+POINT
[Q]::ClientToScreen($h, [ref]$pt) | Out-Null
Write-Output ("%d %d" % ($pt.X, $pt.Y))
""" % proc)
    x, y = out.split()
    return int(x), int(y)


def topmost(proc, on):
    return powershell("""
$h = (Get-Process %s).MainWindowHandle
[Q]::SetWindowPos($h, [IntPtr](%d), 0, 0, 0, 0, %d) | Out-Null
Write-Output "ok"
""" % (proc, HWND_TOPMOST if on else HWND_NOTOPMOST, SWP_NOMOVE_NOSIZE))


def cursor(x, y):
    powershell("[System.Windows.Forms.Cursor]::Position = "
               "New-Object System.Drawing.Point(%d,%d)" % (x, y))


def cursor_pos():
    out = powershell('Write-Output ("%d %d" % '
                     '[System.Windows.Forms.Cursor]::Position.X '
                     '[System.Windows.Forms.Cursor]::Position.Y)')
    x, y = out.split()
    return int(x), int(y)


def widget_center(client, path, proc):
    """控件在屏幕上的中心点（客户区原点 + 逐级子控件偏移）。"""
    segs = path.split("::")
    x = y = 0
    for i in range(1, len(segs)):  # 第 0 级是顶层，它的 x/y 是帧原点，不用
        p = client.widget(path="::".join(segs[:i + 1]), timeout=20.0).properties()
        x += int(p["x"])
        y += int(p["y"])
    last = client.widget(path=path, timeout=20.0).properties()
    ox, oy = client_origin(proc)
    return ox + x + int(last["width"]) // 2, oy + y + int(last["height"]) // 2


def main():
    path, out_dir = sys.argv[1], sys.argv[2]
    port = int(sys.argv[3]) if len(sys.argv) > 3 else 9000
    proc = sys.argv[4] if len(sys.argv) > 4 else "demo_app"

    c = fc.connect(port)
    start = cursor_pos()
    topmost(proc, True)
    try:
        bx, by = widget_center(c, path, proc)
        print("控件中心（屏幕物理坐标）: %d, %d" % (bx, by))

        cursor(start[0], start[1])  # 先确保不在控件上
        time.sleep(0.4)
        normal = fc.grab(c, path, os.path.join(out_dir, "hover_normal.png"))

        cursor(bx, by)
        time.sleep(0.6)             # 等 Enter 事件与重绘
        hovered = fc.grab(c, path, os.path.join(out_dir, "hover_hovered.png"))
    finally:
        cursor(start[0], start[1])
        topmost(proc, False)

    def md5(p):
        with open(p, "rb") as fh:
            return hashlib.md5(fh.read()).hexdigest()

    a, b = md5(normal), md5(hovered)
    print("常态  %s  md5=%s" % (normal, a))
    print("悬停  %s  md5=%s" % (hovered, b))
    print("结论：%s" % ("两帧不同，hover 生效（再自己看图确认形态）"
                     if a != b else "两帧相同——hover 没生效，查 QSS 属性与控件实现"))


if __name__ == "__main__":
    main()
