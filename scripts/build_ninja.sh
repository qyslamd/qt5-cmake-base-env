#!/usr/bin/env bash
#
# Linux / WSL 侧的构建入口，对应 Windows 侧的 scripts/build_ninja.bat。
#
# 与 .bat 有两处刻意差异：
#
#   1. 不走 CMakePresets。预设是给 MSVC 的：名字带 x64 / x86 架构后缀，
#      cache 里有 CMAKE_MSVC_RUNTIME_LIBRARY，binaryDir 又是
#      build/ninja-<类型>-<架构>——与 Windows 侧同名。WSL 与 Windows 常
#      共用同一棵源码树（/mnt/d/...），撞上就直接覆盖对方的 CMakeCache。
#      这里改用 build/linux-ninja-<类型>，两边互不相干。
#   2. 没有 -a/--arch，也没有 -d/--deploy。编译架构只能与本机一致，交叉
#      编译不在支持范围内；部署用的是 windeployqt，Linux 侧没有对应物，
#      打包走 -p（AppImage），与 Windows 的 -p 同义。
#
# 用法见 -h。

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

BUILD_TYPE=""
TARGET=""
ACTION=""
CONFIGURE_ONLY=0

die() {
    echo "错误：$*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
用法：
  旧式：  build_ninja.sh [debug|relwithdebinfo|release] [package]
  新式：  build_ninja.sh -t <类型> [-m <目标>] [-p] [-c]

选项：
  -t, --type <类型>      构建类型：debug / relwithdebinfo / release（默认 debug）
  -m, --target <目标>    只构建指定目标（如 demo_app），省略则全量
  -p, --package          构建后打 AppImage（make_appimage 目标）
  -c, --configure        只跑 CMake configure，不构建
  -h, --help             显示本帮助

与 Windows 侧的差异：
  没有 -a/--arch  ——  Linux 侧只做本机架构，交叉编译不在支持范围内
  没有 -d/--deploy —— windeployqt 是 Windows 专属；Linux 侧部署即打包，用 -p

构建目录为 build/linux-ninja-<类型>，与 Windows 侧的
build/ninja-<类型>-<架构> 分开，共用同一棵源码树时不会互相覆盖
CMakeCache。

跑测试：ctest --test-dir build/linux-ninja-<类型> --output-on-failure

示例：
  ./scripts/build_ninja.sh
  ./scripts/build_ninja.sh relwithdebinfo package
  ./scripts/build_ninja.sh -t debug -m demo_app
  ./scripts/build_ninja.sh -t relwithdebinfo -p
EOF
}

# ---------------------------------------------------------------------------
# 参数解析
# ---------------------------------------------------------------------------
# 第一个参数不以 - 开头即为旧式位置参数；两种形式不混用
if [ $# -gt 0 ] && [ "${1:0:1}" != "-" ]; then
    BUILD_TYPE="$1"
    shift
    case "${1:-}" in
        "")
            ;;
        package)
            ACTION=package
            shift
            ;;
        deploy)
            die "Linux 侧没有 windeployqt，旧式的 deploy 不适用；打包请用 package（AppImage）"
            ;;
        *)
            die "无法识别的参数：$1"
            ;;
    esac
    if [ $# -gt 0 ]; then
        die "无法识别的参数：$1"
    fi
else
    while [ $# -gt 0 ]; do
        case "$1" in
            -h|--help)
                usage
                exit 0
                ;;
            -t|--type)
                [ $# -ge 2 ] || die "-t/--type 后面要跟一个构建类型"
                BUILD_TYPE="$2"
                shift 2
                ;;
            --type=*)
                BUILD_TYPE="${1#--type=}"
                shift
                ;;
            -m|--target)
                [ $# -ge 2 ] || die "-m/--target 后面要跟一个目标名"
                TARGET="$2"
                shift 2
                ;;
            --target=*)
                TARGET="${1#--target=}"
                shift
                ;;
            -p|--package)
                ACTION=package
                shift
                ;;
            -c|--configure)
                CONFIGURE_ONLY=1
                shift
                ;;
            -d|--deploy)
                die "Linux 侧没有 windeployqt，-d/--deploy 不适用；打包请用 -p（AppImage）"
                ;;
            -a|--arch|--arch=*)
                die "Linux 侧只做本机架构（$(uname -m)），没有 -a/--arch"
                ;;
            *)
                echo "无法识别的参数：$1" >&2
                usage >&2
                exit 1
                ;;
        esac
    done
fi

# 构建类型 → CMake 配置名。类型名沿用 .bat 的小写写法，
# 传给 CMake 的用规范拼写
BUILD_TYPE="${BUILD_TYPE:-debug}"
case "$BUILD_TYPE" in
    debug)
        CMAKE_BUILD_TYPE=Debug
        ;;
    relwithdebinfo)
        CMAKE_BUILD_TYPE=RelWithDebInfo
        ;;
    release)
        CMAKE_BUILD_TYPE=Release
        ;;
    *)
        echo "无法识别的构建类型：$BUILD_TYPE" >&2
        usage >&2
        exit 1
        ;;
esac

BUILD_DIR="build/linux-ninja-${BUILD_TYPE}"

echo "构建类型：${BUILD_TYPE}（${CMAKE_BUILD_TYPE}）"
echo "构建目录：${BUILD_DIR}"
if [ -n "$TARGET" ]; then
    echo "构建目标：${TARGET}"
fi

# ---------------------------------------------------------------------------
# 配置
# ---------------------------------------------------------------------------
if ! cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE"; then
    die "CMake configure 失败"
fi
echo "Configure OK"

if [ "$CONFIGURE_ONLY" -eq 1 ]; then
    echo "只做 configure，跳过构建。"
    exit 0
fi

# ---------------------------------------------------------------------------
# 构建
# ---------------------------------------------------------------------------
if [ -n "$TARGET" ]; then
    if ! cmake --build "$BUILD_DIR" --target "$TARGET"; then
        die "构建失败"
    fi
else
    if ! cmake --build "$BUILD_DIR"; then
        die "构建失败"
    fi
fi

echo "构建完成！"

# ---------------------------------------------------------------------------
# 打包（对应 Windows 侧的 -p：windeployqt + ISCC）
# ---------------------------------------------------------------------------
if [ "$ACTION" = "package" ]; then
    echo
    echo "打包 AppImage ..."
    if ! cmake --build "$BUILD_DIR" --target make_appimage; then
        die "AppImage 打包失败"
    fi
    echo "打包完成！"
fi
