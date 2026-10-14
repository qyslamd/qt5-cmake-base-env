"""给单语句 if/for/while/else 体补花括号（自底向上，避免悬垂 else 改归属）。

用法: python add_braces.py <file.cpp> [--dry]
"""

import re
import sys

COND = re.compile(r"^(\s*)((?:else\s+)?(?:if|for|while)\b.*\))\s*(//.*)?$")
BS = chr(92)
QUOTE = chr(34)


def body_end(lines, start):
    """从 start 行（体首行）起，找这条语句的结束行号。"""
    depth_paren = 0
    depth_brace = 0
    for i in range(start, len(lines)):
        line = lines[i]
        in_str = False
        j = 0
        while j < len(line):
            ch = line[j]
            if in_str:
                if ch == BS:
                    j += 2
                    continue
                if ch == QUOTE:
                    in_str = False
            elif ch == QUOTE:
                in_str = True
            elif ch == "/" and j + 1 < len(line) and line[j + 1] == "/":
                break
            elif ch == "(":
                depth_paren += 1
            elif ch == ")":
                depth_paren -= 1
            elif ch == "{":
                depth_brace += 1
            elif ch == "}":
                depth_brace -= 1
                if depth_brace == 0 and i > start:
                    return i
            elif ch == ";" and depth_paren == 0 and depth_brace == 0:
                return i
            j += 1
    return len(lines) - 1


def process(path, dry):
    with open(path, encoding="utf-8") as fh:
        lines = fh.read().replace(chr(13) + chr(10), chr(10)).split(chr(10))

    hits = []
    for i, line in enumerate(lines):
        m = COND.match(line)
        if not m:
            continue
        nxt = i + 1
        if nxt >= len(lines):
            continue
        stripped = lines[nxt].strip()
        if not stripped or stripped.startswith("{") or stripped.startswith("}"):
            continue
        hits.append((i, m))

    for i, m in reversed(hits):  # 自底向上：内层先包
        end = body_end(lines, i + 1)
        indent, cond = m.group(1), m.group(2)
        tail = m.group(3)
        lines[i] = "%s%s {%s" % (indent, cond, ("  " + tail) if tail else "")
        lines.insert(end + 1, "%s}" % indent)

    if dry:
        print("%s: 需补 %d 处" % (path, len(hits)))
        return
    with open(path, "w", encoding="utf-8", newline="") as fh:
        fh.write(chr(10).join(lines))
    print("%s: 补了 %d 处" % (path, len(hits)))


if __name__ == "__main__":
    process(sys.argv[1], "--dry" in sys.argv)
