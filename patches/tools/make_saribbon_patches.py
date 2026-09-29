"""从 pristine 基线生成 SARibbon / QWindowKit 的补丁文件。

用法——**工作目录必须是 pristine 副本的根**，脚本按相对路径改文件：

    mkdir -p temp/patchgen && cd temp/patchgen
    tar xzf ../../3rdparty/SARibbon-2.5.7.tar.gz
    tar xzf ../../3rdparty/qwindowkit-1.5.0.tar.gz
    cd SARibbon-2.5.7
    python -c "import pathlib,sys;\
[p.write_bytes(p.read_bytes().replace(b'\r\n', b'\n')) for p in pathlib.Path('.').rglob('*') if p.is_file()]"
    git init -q && git add -A && git commit -qm base-lf      # 基线须为 LF 行尾
    python ../../../patches/tools/make_saribbon_patches.py

基线的 LF 归一不能省：上游 SARibbon 的 CMakeLists 是 CRLF，而
patch_dependencies.cmake 在 apply 前会把目标归一化成 LF，两侧不一致时补丁的
上下文行对不上（该步骤靠 `git apply --ignore-whitespace` 兜底，但那只是兜底）。

每条补丁做完写一次 diff、提交一次，下一条从新状态起算，所以补丁必须按序号
顺序应用。重跑前先 `git reset --hard base-lf`——脚本跑完停在最后一版的状态。
"""

import pathlib
import re
import subprocess

REPO = pathlib.Path(__file__).resolve().parents[2]
OUT_SARIBBON = REPO / "patches" / "SARibbon"
OUT_QWK = REPO / "patches" / "QWindowKit"


def edit(rel, pairs=(), regex=None):
    p = pathlib.Path(rel)
    data = p.read_bytes()
    for old, new in pairs:
        old_b, new_b = old.encode("utf-8"), new.encode("utf-8")
        assert data.count(old_b) == 1, (rel, old[:50], data.count(old_b))
        data = data.replace(old_b, new_b)
    if regex is not None:
        pat, repl = regex
        data, n = re.subn(pat.encode("utf-8"), repl.encode("utf-8"), data, flags=re.DOTALL)
        assert n == 1, (rel, "regex matched", n)
    p.write_bytes(data)


def commit(msg):
    subprocess.run(["git", "add", "-A"], check=True)
    subprocess.run(
        ["git", "-c", "user.email=a@b", "-c", "user.name=x", "commit", "-qm", msg],
        check=True,
    )


def save(out_dir, name):
    diff = subprocess.run(["git", "diff"], capture_output=True, check=True).stdout
    assert diff, "empty diff"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / name).write_bytes(diff.replace(b"\r\n", b"\n"))
    commit(name)
    print(f"wrote {out_dir.name}/{name}  ({len(diff)} bytes)")


# ---------------------------------------------------------------- SARibbon
# 0001：只用 Qt5。上游做的是版本无关查找（包名 QT，认 QT_DIR），与本仓
# local.cmake 只给 Qt5_DIR 的写法对不上，机器上装了 Qt6 还会选中它。
edit(
    "CMakeLists.txt",
    [
        (
            "find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)\n",
            "# 本仓只用 Qt5：上游这里是版本无关查找（包名 QT，认 QT_DIR），"
            "与本仓\n"
            "# local.cmake 只提供 Qt5_DIR 的写法对不上，机器上装了 Qt6 还会选中它。\n"
            "# 下面几句等价于那次查找的产物——SARibbon 后面按 QT_VERSION_MINOR\n"
            "# 判断能否使用 frameless，缺了会展开成 if(LESS 14) 直接报错\n"
            "string(REPLACE \".\" \";\" _saribbon_qt5_parts \"${Qt5_VERSION}\")\n"
            "list(GET _saribbon_qt5_parts 0 QT_VERSION_MAJOR)\n"
            "list(GET _saribbon_qt5_parts 1 QT_VERSION_MINOR)\n"
            "list(LENGTH _saribbon_qt5_parts _saribbon_qt5_count)\n"
            "if(_saribbon_qt5_count GREATER 2)\n"
            "    list(GET _saribbon_qt5_parts 2 QT_VERSION_PATCH)\n"
            "else()\n"
            "    set(QT_VERSION_PATCH 0)\n"
            "endif()\n"
            "set(QT_VERSION \"${Qt5_VERSION}\")\n",
        )
    ],
)
save(OUT_SARIBBON, "0001-qt5-only.patch")

# 0002：抬 cmake_minimum_required。3.5 在 CMake 3.31 下会打 deprecation 警告，
# 且 policy 版本过低会让上游写的 if() 语义与预期不同。
edit("CMakeLists.txt", [("cmake_minimum_required(VERSION 3.5)\n", "cmake_minimum_required(VERSION 3.15)\n")])
edit(
    "src/SARibbonBar/CMakeLists.txt",
    [("cmake_minimum_required(VERSION 3.5)\n", "cmake_minimum_required(VERSION 3.15)\n")],
)
save(OUT_SARIBBON, "0002-bump-cmake-minimum-version.patch")

# 0003：QWindowKit 走 in-tree 目标，不走 find_package
edit(
    "CMakeLists.txt",
    regex=(
        r"# QWindowKit的依赖\n.*?\n    endif\(\)\nendif\(\)\n",
        "# QWindowKit的依赖\n"
        "# 本仓把 QWindowKit 当子项目源码内嵌（add_subdirectory），不走 find_package：\n"
        "# 它的目标在配置期就已存在，直接检查即可。上游那套“依次找 QWindowKit_DIR /\n"
        "# 本地安装目录 / 系统路径”的查找在这里没有意义，且会误选中系统里另一份\n"
        "if(_SARIBBON_USE_FRAMELESS_LIB)\n"
        "    if(TARGET QWindowKit::Widgets)\n"
        '        message(STATUS "QWindowKit found (built in-tree by add_subdirectory)")\n'
        "    else()\n"
        "        set(_SARIBBON_USE_FRAMELESS_LIB OFF)\n"
        '        message(WARNING "QWindowKit not found - disabling frameless support.")\n'
        "    endif()\n"
        "endif()\n",
    ),
)
edit(
    "src/SARibbonBar/CMakeLists.txt",
    [("if(_SARIBBON_USE_FRAMELESS_LIB)\n    find_package(QWindowKit REQUIRED)\n", "if(_SARIBBON_USE_FRAMELESS_LIB)\n")],
)
save(OUT_SARIBBON, "0003-frameless-as-in-tree-target.patch")

# 0004：不登记 install EXPORT
edit(
    "src/SARibbonBar/CMakeLists.txt",
    [
        (
            "install(TARGETS ${SARIBBON_LIB_NAME}\n    EXPORT ${SARIBBON_LIB_NAME}Targets\n",
            "# 不登记 EXPORT：本仓把 SARibbon 当子项目源码内嵌，不做 find_package 分发；\n"
            "# 且 QWindowKit 的目标不在本 export set 里，留着会在 install 阶段报错\n"
            "install(TARGETS ${SARIBBON_LIB_NAME}\n",
        ),
        (
            "install(EXPORT ${SARIBBON_LIB_NAME}Targets\n"
            "    FILE ${SARIBBON_LIB_NAME}Targets.cmake\n"
            "    NAMESPACE ${SARIBBON_LIB_NAME}::\n"
            "    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${SARIBBON_LIB_NAME}\n"
            ")\n\n",
            "",
        ),
    ],
)
save(OUT_SARIBBON, "0004-drop-install-export.patch")

print("SARibbon patches done")
