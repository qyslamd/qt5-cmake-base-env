# OSG GLCORE 源码集成方案

- 日期：2026-11-05
- 状态：已实施；x64/x86 RelWithDebInfo 构建与 `osg_demo` 截图验证均通过
- 审查报告：`docs/plan/subagent审查结果.md`
- 目标：把 OpenSceneGraph 3.6.5 以 GLCORE 模式接入本仓，源码与依赖由本仓
  git 管理，使用本仓现有的解压与补丁机制

---

## 1. 问题与目标

### 1.1 背景

`D:\trae_workspace\qt-osg-base-env\osg-compile-glcore` 已经验证过
OSG 3.6.5 的 GLCORE 构建，以及 Qt Widgets/QML 的集成路径。现有验证工程
使用的是独立源码树、独立 install 前缀和一套独立的构建脚本。

本仓需要的是另一种形态：

- OSG 不是外部预编译 SDK，而是本仓 `3rdparty/` 下的源码依赖；
- 与本仓其它依赖一样，首次 configure 时解压，补丁在 configure 阶段重放；
- 与本仓的 zlib 共用同一份实现，避免第二份 zlib；
- 同时支持 x86 和 x64；
- 不破坏现有 `demo_app`、示例、测试和打包流程。

### 1.2 目标

1. 新增 `BUILD_OSG` 开关，开启后在主构建中编译 OSG 3.6.5 GLCORE。
2. OSG 及其依赖（zlib、libpng、libjpeg-turbo、freetype）源码进入 git 管理。
3. 复用 `cmake/extract_dependencies.cmake`、`cmake/patch_dependencies.cmake`
   与 `patches/`，不引入第二套补丁机制。
4. OSG 使用本仓已有的 `zlibstatic`，不再单独编译或查找系统 zlib。
5. 提供最小的编译/运行验证路径，确认 GLCORE、插件、x86/x64 都可用。
6. 明确 Qt GL 头与 GLCORE OSG 头的隔离规则，避免后续使用者踩
   `GL/gl.h` 与 `GL/glcorearb.h` 冲突。

### 1.3 非目标

- 本期不把 OSG 链进 `demo_app`，也不默认打进主安装包。
- 本期不实现完整的 OSG 场景查看器，只提供集成与最小验证。
- 本期不做 QML/QQuick 集成；QML 方案已在实验工程验证，另立任务。
- 本期不验证 Linux/WSL；先保证 Windows + MSVC + Qt5 的 x86/x64。
- 不采用实验工程的预编译 install 目录作为正式依赖。
- 不做 GL2/GL3 双 Profile；本仓 OSG 固定 GLCORE。

---

## 2. 现状与约束

### 2.1 本仓现有依赖机制

- `3rdparty/*.tar.gz` 入库，解压目录被 `.gitignore` 的 `3rdparty/*/` 忽略。
- `cmake/extract_dependencies.cmake` 的 `DEPENDENCY_ARCHIVES` 是解压清单，
  也是 `scripts/clean_deps.*` 的来源。
- `cmake/patch_dependencies.cmake` 在 configure 阶段用 `git apply` 打补丁，
  成功后写 `.patch_applied_*` 标记，保证幂等。
- 依赖的构建产物统一进 `build/lib`、`build/bin`；第三方 install 规则通常
  需要屏蔽，避免污染本仓安装前缀。
- 现有 zlib 由 `cmake/crashpad/crashpad.cmake` 以 `EXCLUDE_FROM_ALL` 引入，
  目标名 `zlibstatic`，并建立 `ZLIB::ZLIB` 别名；`src/core` 的 crashpad
  依赖已经在用它。

### 2.2 实验工程已经验证的结论

参考 `D:\trae_workspace\qt-osg-base-env\osg-compile-glcore`：

- OSG 3.6.5 可以在 VS2019 + Ninja 下编出 x86/x64 GLCORE 版本；
- 关键参数是 `OPENGL_PROFILE=GLCORE`、`OPENGL_HEADER1`、`GLCORE_ROOT`；
- 依赖顺序是 zlib → libpng → libjpeg-turbo → freetype → OSG；
- OSG install 不会自动带依赖 DLL，需要额外复制；
- OSG 插件位于 `bin/osgPlugins-3.6.5/`，运行时通过
  `OSG_LIBRARY_PATH` 指向 `bin`；
- Qt 的 `QOpenGLWidget` 与 OSG GLCORE 头不能出现在同一个编译单元，
  必须按编译单元隔离；
- 该工程只验证了 Release；本仓发布口径是 RelWithDebInfo，需要额外处理
  OSG 的 `rd` 后缀；
- OSG 编译内存峰值高，实验工程使用 `--parallel 2`，默认全核有被系统
  终止的记录。

### 2.3 本仓需要额外处理的点

| 点 | 影响 | 处理方向 |
| --- | --- | --- |
| zlib 重复 | OSG 自带 `FindZLIB`，容易找到系统 zlib 或再编一份 | 复用 `zlibstatic`，用变量注入 OSG |
| CMake 4 策略 | OSG/libjpeg/freetype 的 `cmake_minimum_required` 低于 3.5 | 走 `patches/` 抬到 3.5 |
| RelWithDebInfo 后缀 | OSG 默认给 RelWithDebInfo 产物加 `rd` | 进 OSG 作用域前清空该后缀 |
| OSG install 规则 | 会把 OSG 头文件、库、插件装进本仓安装前缀 | 本期不把 OSG 纳入 `cmake --install`；不依赖 `CMAKE_SKIP_INSTALL_RULES` |
| OSG 全局输出目录 | OSG 在自己的目录作用域内改 `CMAKE_*_OUTPUT_DIRECTORY` | 限定在 `add_subdirectory` 内，父作用域不受影响 |
| 编译内存 | OSG + 插件全量编译，7 GB 内存容易 OOM | 用 `build_ninja.* -j 2` 限制并行度 |
| Qt GL 头冲突 | 同一 `.cpp` 同时包含 Qt GL 头与 OSG GLCORE 头会重定义 | 强制分编译单元，示例与文档写清楚 |
| 许可证 | OSG 是 OSGPL（LGPL 系），不是 MIT | 动态链接；许可证登记与随包分发 |

---

## 3. 方案选项

### 3.1 依赖接入方式

| 方案 | 做法 | 评价 |
| --- | --- | --- |
| A. 源码 `add_subdirectory` | 解压后在主工程内直接 `add_subdirectory` | 推荐。单次 configure/build，目标可直接链接，最符合本仓现有依赖机制 |
| B. `ExternalProject` 超级构建 | 依赖和 OSG 各自独立 configure/build/install | 不推荐。构建分两段，目标传递、调试和 ctest 都更绕；与现有依赖风格不一致 |
| C. 预编译 SDK | 直接引用实验工程的 `install/.../Release-glcore` | 不推荐。不满足源码入库与 x86/x64 自建要求，也无法保证编译器/运行库一致 |

### 3.2 OSG 链接形态

| 方案 | 评价 |
| --- | --- |
| OSG 动态库 + 依赖静态库 | 推荐。OSG 的插件机制依赖动态库；zlib/libpng/jpeg/freetype 静态进 OSG DLL 与插件，运行时只需部署 OSG 自己的 DLL 和插件 |
| OSG 静态库 + 插件动态 | 不推荐。插件注册、符号导出和 `OSG_LIBRARY` 宏更复杂，最终 exe 也会更大 |
| OSG 全动态 + 依赖全动态 | 可行但运行时要多带 zlib/libpng/jpeg/freetype DLL，且容易出现位数/版本混用；不作为首选 |

### 3.3 zlib 方案

| 方案 | 评价 |
| --- | --- |
| 复用现有 `zlibstatic` | 推荐。崩溃捕获与 OSG 共用同一份 zlib，版本与编译选项一致 |
| 给 OSG 单独编一份 zlib | 不推荐。两份 zlib 的静态状态、符号和运行时依赖都会增加排查成本 |
| 使用系统 zlib | 不推荐。破坏本仓“源码固定、离线可构建”的约定，x86/x64 也容易错配 |

### 3.4 安装与部署

| 方案 | 评价 |
| --- | --- |
| 屏蔽 OSG install，使用构建树 | 推荐。OSG 只在构建树内提供目标和运行时，主安装包不被无谓撑大 |
| 允许 OSG install 到主前缀 | 不推荐。会污染 `install/`，且与 AppImage/Inno 的现有布局冲突 |
| 把 OSG 链进 `demo_app` 并默认打包 | 本期不做。OSG DLL/插件体积大，等确有业务使用时再接入 |

---

## 4. 推荐方案

采用 **A + 动态 OSG + 静态依赖 + 复用 zlibstatic + 构建树内使用**。

### 4.1 目录与 git 管理

```text
3rdparty/
├── OpenSceneGraph-OpenSceneGraph-3.6.5.tar.gz   入 git
├── libpng-1.6.43.tar.gz                          入 git
├── libjpeg-turbo-2.1.4.tar.gz                    入 git
├── freetype-2.12.1.tar.gz                        入 git
├── zlib-v1.2.11.tar.gz                           已入库
├── glcore/                                       入 git（当前被忽略）
│   ├── GL/glcorearb.h
│   └── KHR/khrplatform.h
├── OpenSceneGraph-OpenSceneGraph-3.6.5/          解压产物，不入 git
├── libpng-1.6.43/                                解压产物，不入 git
├── libjpeg-turbo-2.1.4/                          解压产物，不入 git
└── freetype-2.12.1/                              解压产物，不入 git

patches/
├── OpenSceneGraph/
│   ├── 0001-bump-cmake-minimum-version.patch
│   └── 0002-guard-uninstall-target.patch
├── libpng/0001-remove-unused-asm-language.patch
├── libjpeg-turbo/
│   ├── 0001-bump-cmake-minimum-version.patch
│   └── 0002-guard-uninstall-target.patch
├── freetype/0001-bump-cmake-minimum-version.patch
└── zlib/                                         已有 0001/0002

cmake/osg/osg.cmake                               新增，统一接入
examples/osg_demo/                                从 blender-test 移入，不含 bpy
```

`.gitignore` 需要给 `3rdparty/glcore/` 开例外，写法与现有
`!3rdparty/QCustomPlot/` 一致；`examples/osg_demo/output/` 也要开例外，
因为底座规则默认忽略所有 `output/`。

说明：本仓的“源码由 git 管理”按现有约定实现为**压缩包 + 补丁入库，
解压目录不入库**。如果要求把解压后的完整源码目录也入库，仓库会增大
数十 MB，并失去“换版本只需换 tar 包”的清晰边界；本方案不采用。

### 4.2 顶层 CMake 接入

在 `CMakeLists.txt` 的构建开关区增加：

```cmake
option(BUILD_OSG "构建 OpenSceneGraph GLCORE 支持" OFF)
```

在 `include(cmake/crashpad/crashpad.cmake)` 之后、`add_subdirectory(src)`
之前引入：

```cmake
if(BUILD_OSG)
    include(${CMAKE_SOURCE_DIR}/cmake/osg/osg.cmake)
endif()
```

放在 crashpad 之后的原因：`crashpad.cmake` 会先建立 `zlibstatic` 与
`ZLIB::ZLIB`，OSG 接入直接复用，不重复编译 zlib。

`BUILD_OSG=OFF` 时，4 个 OSG 相关压缩包不应进入无条件解压清单；在
`extract_dependencies.cmake` 里按 `BUILD_OSG` 条件追加。否则默认 OFF 仍会
在每个新构建目录解压 OSG 与依赖，违背“默认不背 OSG 成本”的初衷。
`clean_deps` 由当前配置的解压清单生成，切到 OFF 后旧构建留下的 OSG
解压目录需要手工清理或先用 ON 配置生成清理脚本。

### 4.3 `cmake/osg/osg.cmake` 的职责

建议按下面的顺序实现。**所有 `option()` / `set(... CACHE ...)` 变量都用
`CACHE ... FORCE`**；普通变量会被子项目的策略 OLD 行为顶掉（见 §4.4）。

1. **平台闸门**
   - 本期只在 `WIN32` 且 MSVC 下启用；其它平台打印 STATUS 后跳过，保证
     Linux/WSL 现有构建不被破坏。
2. **校验源码**
   - 检查 OSG、libpng、libjpeg-turbo、freetype、glcore 解压目录存在；
     缺失时给出“先跑 configure 解压”的明确错误。
3. **断言 zlib**
   - `if(NOT TARGET zlibstatic)` 直接 FATAL，提示 crashpad 的 zlib 引入
     没有生效，避免静默去找系统 zlib；只断言 `ZLIB::ZLIB` 不够，因为别名
     可能来自系统的 `find_package(ZLIB)`。
4. **构建 libpng**
   - 在 `add_subdirectory` 之前用 FORCE 设置：
     `PNG_SHARED=OFF`、`PNG_STATIC=ON`、`PNG_TESTS=OFF`、
     `PNG_TOOLS=OFF`、`PNG_FRAMEWORK=OFF`；
   - `add_subdirectory(... EXCLUDE_FROM_ALL)` 到
     `build/3rdparty/osg-deps/libpng`，避免测试/工具目标进入 `all`。
5. **构建 libjpeg-turbo**
   - 用 FORCE 设置 `ENABLE_SHARED=OFF`、`ENABLE_STATIC=ON`、
     `WITH_TURBOJPEG=OFF`、`WITH_SIMD=OFF`；
   - 该版本没有 `WITH_TOOLS` 选项，四类命令行工具会无条件生成；
     用 `EXCLUDE_FROM_ALL` 把它们排除出默认构建；
   - SIMD 默认关闭，避免把 NASM 变成新的外部工具依赖；需要性能时再单独
     开启并检测 `nasm`。
6. **构建 freetype**
   - 用 FORCE 设置 `BUILD_SHARED_LIBS=OFF`、
     `FT_DISABLE_BZIP2=ON`、`FT_DISABLE_BROTLI=ON`、
     `FT_DISABLE_HARFBUZZ=ON`；
   - zlib / PNG 指向树内目标；用 `EXCLUDE_FROM_ALL` 引入。
7. **配置 OSG**
   - 全部用 FORCE：
     `OPENGL_PROFILE=GLCORE`、`OPENGL_HEADER1=#include <GL/glcorearb.h>`、
     `GLCORE_ROOT=${3RDPARTY_DIR}/glcore`、`DYNAMIC_OPENSCENEGRAPH=ON`、
     `OSG_MSVC_VERSIONED_DLL=OFF`、`OSG_USE_UTF8_FILENAME=ON`、
     `OSG_TEXT_USE_FONTCONFIG=OFF`、`BUILD_OSG_EXAMPLES=OFF`、
     `BUILD_OSG_APPLICATIONS=ON`、`BUILD_OSG_PLUGINS=ON`、
     `USE_3RDPARTY_BIN=OFF`；
   - `BUILD_OSG_APPLICATIONS=ON` 实际会编 6 个应用：`osgviewer`、
     `osgarchive`、`osgconv`、`osgfilecache`、`osgversion`、`present3D`；
     本期只验收 `osgversion` / `osgconv`；
   - 关闭实验工程里已经关掉的可选依赖，名称必须与 `find_package` 完全一致：
     `FFmpeg`、`GDAL`、`CURL`、`TIFF`、`GIFLIB`、`OpenEXR`、`ilmbase`、
     `COLLADA`、`FBX`、`Jasper`、`OpenCascade`、`LibVNCServer`、`DCMTK`、
     `GStreamer`、`GLIB`、`DirectShow`、`SDL`、`SDL2`、`Poppler-glib`、
     `RSVG`、`GtkGl`、`DirectInput`、`NVTT`、`ZeroConf`、`LIBLAS`、
     `Lua52`、`Lua51`、`Inventor`、`QuickTime`；
   - `USE_3RDPARTY_BIN` 必须 FORCE 为 OFF；同时提示或清理
     `OSG_3RDPARTY_DIR` 环境变量，避免它的 `SEARCH_3RDPARTY` 覆盖注入。
8. **处理工程后缀**
   - 四个 `CMAKE_*_POSTFIX` 都是缓存项，必须在 OSG 之前 FORCE 成本仓口径
     （`d`、空、空、空），不能只设普通变量；
   - 尤其 `CMAKE_RELWITHDEBINFO_POSTFIX` 为空，避免生成
     `osgrd.dll` / `osgDBrd.dll`；Debug 的 `d` 后缀保留；
   - 接入前后保存/恢复原值，避免影响后面的 SARibbon 等同名缓存项。
9. **安装边界**
   - 本期不把 OSG 纳入 `cmake --install` 的发布路径；Windows 打包走
     Inno Setup，不调用它；
   - **不把 `CMAKE_SKIP_INSTALL_RULES` 当作已验证机制**：本仓 FluentUI 的
     先例已经留下悬空 `include()`，`cmake --install` 会失败；
   - 如果将来确实要隔离 OSG install，先写一个 `cmake --install
     <构建目录> --prefix <temp>` 冒烟测试，再考虑在 OSG 包装目录里遮蔽
     `install()` 或使用专用 install 前缀。
10. **限制编译并发**
    - 不在 OSG 接入层另设 Ninja job pool；
    - 并行度统一由 `scripts/build_ninja.bat` / `build_ninja.sh` 的
      `-j <N>` / `--jobs <N>` 控制；
    - OSG 首次完整构建建议使用 `-j 2`，避免 7 GB 内存被全核编译打满。
11. **暴露目标**
    - 先给真实目标 `osg`、`osgDB`、`osgGA`、`osgUtil`、`osgViewer`、
      `osgText`、`OpenThreads` 补 `INTERFACE` include 目录：
      `${OSG_SOURCE_DIR}/include`、`${OSG_BINARY_DIR}/include` 和
      `${3RDPARTY_DIR}/glcore`；
    - **再**建立 `OSG::...` 别名；别名上不能再设 include 目录；
    - 依赖的构建目录（至少 zlib 的 `zconf.h`、libjpeg 的 `jconfig.h`、
      freetype 的 `ftconfig.h`）在 OSG 作用域内用 `include_directories`
      补上，不能只靠链接目标的 usage requirements 兜底；
    - 本仓消费者只链 `OSG::osgViewer` 等别名，不直接写裸路径。
12. **导出运行期变量**
    - `OSG_RUNTIME_DIR=${OSG_BINARY_DIR}/bin`
    - `OSG_PLUGIN_DIR=${OSG_RUNTIME_DIR}/osgPlugins-3.6.5`
    - `OSG_LIBRARY_PATH=${OSG_RUNTIME_DIR}`
    - 供示例、run 脚本和部署目标使用。

### 4.4 依赖变量注入

OSG 3.6.5 仍用旧的变量式查找，不认现代 imported target。接入层必须在
**任何依赖 `add_subdirectory` 之前**（包括 libpng，不只是 OSG）把 Find
变量指向树内目标，例如：

```cmake
set(ZLIB_FOUND TRUE)
set(ZLIB_LIBRARY zlibstatic)
set(ZLIB_LIBRARIES zlibstatic)
set(ZLIB_INCLUDE_DIR "${3RDPARTY_DIR}/zlib-v1.2.11")

set(PNG_FOUND TRUE)
set(PNG_LIBRARY png_static)
set(PNG_LIBRARIES "png_static;zlibstatic")
set(PNG_PNG_INCLUDE_DIR "${3RDPARTY_DIR}/libpng-1.6.43")

set(JPEG_FOUND TRUE)
set(JPEG_LIBRARY jpeg-static)
set(JPEG_LIBRARIES jpeg-static)
set(JPEG_INCLUDE_DIR "${3RDPARTY_DIR}/libjpeg-turbo-2.1.4")

set(FREETYPE_FOUND TRUE)
set(FREETYPE_LIBRARY freetype)
set(FREETYPE_LIBRARIES freetype)
set(FREETYPE_INCLUDE_DIR_ft2build "${3RDPARTY_DIR}/freetype-2.12.1/include")
set(FREETYPE_INCLUDE_DIR_freetype2 "${3RDPARTY_DIR}/freetype-2.12.1/include")
set(FREETYPE_INCLUDE_DIRS
    "${3RDPARTY_DIR}/freetype-2.12.1/include")
```

注意点：

- **Find 变量可以用普通变量**：`find_path` / `find_library` 遇到已定义变量
  会跳过搜索；`FindPNG` 也有 `if(NOT PNG_LIBRARY)` 保护。
- **选项/缓存变量不能用普通变量**：`option()` 和 `set(... CACHE ...)` 在
  子项目的策略 OLD 下会忽略父作用域普通变量，必须 `CACHE ... FORCE`。
  这两类变量分开处理，不能把 §4.4 的写法套到 §4.3 的选项上。
  也可以在接入层开头设 `CMAKE_POLICY_DEFAULT_CMP0077=NEW` 与
  `CMAKE_POLICY_DEFAULT_CMP0126=NEW`，但逐项 FORCE 更确定，也是本仓现有
  依赖的写法。
- `zlibstatic` 是 `crashpad.cmake` 里已有的真实目标，`ZLIB::ZLIB` 只是它的
  别名；变量里写 `zlibstatic` 更稳。
- `zconf.h`、`jconfig.h`、`ftconfig.h` 等生成头在各自构建目录；除了链接
  目标，还要在 OSG 作用域内把这些构建目录加入 include。
- `FindPNG` 会把 `PNG_LIBRARY` 与 `ZLIB_LIBRARY` 串进 `PNG_LIBRARIES`；
  上面显式写成目标列表，避免它去找系统 png/zlib。
- 如果实测发现某个 `Find*` 模块仍然覆盖变量，再补一条
  `patches/OpenSceneGraph/0002-in-tree-dependencies.patch`，在 OSG 的
  `find_package` 之前短路。**先不加补丁，优先用变量注入解决**；只有
  configure 实测证明不行才打补丁。
- 已配置过的构建目录可能把 `OSG_GL*_AVAILABLE`、`OPENGL_PROFILE` 等缓存成
  GL2 组合；改完方案后要删 `CMakeCache.txt` 或用
  `cmake -U 'OSG_GL*' -U OPENGL_PROFILE -U OSG_MSVC_VERSIONED_DLL ...`
  清理后再验证。

实测补充：

- `PNG_PNG_INCLUDE_DIR`、`ZLIB_INCLUDE_DIR`、`JPEG_INCLUDE_DIR` 等旧构建
  目录可能残留空缓存；除了普通变量，还要在接入层用 `CACHE PATH ... FORCE`
  钉到树内路径，否则 `FindPNG` 会报 missing include。
- libpng 1.6.43 的 `project(... LANGUAGES C ASM)` 在 MSVC 下会要求
  `CMAKE_ASM_COMPILE_OBJECT`，而实际只用 C；用
  `patches/libpng/0001-remove-unused-asm-language.patch` 去掉 ASM。
- libjpeg-turbo 和 OSG 都无条件创建 `uninstall` 目标，同目录接入时冲突；
  两边各加一条 guard 补丁。
- libpng/freetype 的 `install(EXPORT)` 会引用未导出的 `zlibstatic`，generate
  阶段报错；只在这两个子树内设 `CMAKE_SKIP_INSTALL_RULES` 让 generate 通过，
  不把 `cmake --install` 作为发布/验收路径。
- libjpeg 旧缓存可能留下空的 `CMAKE_INSTALL_DOCDIR`，导致
  `install(FILES ... DESTINATION "")` 配置失败；接入层先清掉该缓存项。

### 4.5 构建类型与产物命名

- 本仓统一使用 `RelWithDebInfo` 做验证/发布；四个
  `CMAKE_*_POSTFIX` 都是缓存项，必须在 OSG 之前用
  `CACHE ... FORCE` 设成本仓口径（`d`、空、空、空）。只设普通变量无效，
  且 OSG 会把 `CMAKE_RELWITHDEBINFO_POSTFIX` 写成 `rd`，生成
  `osgrd.dll` / `osgDBrd.dll`。
- `SET(... CACHE ...)` 会外溢到后面的 SARibbon 等依赖；接入前后要保存/
  恢复原值，或统一在顶层先钉死四个后缀。
- Debug 构建仍使用 OSG 默认的 `d` 后缀；Debug 与 RelWithDebInfo 的 DLL
  不能混用，运行时 PATH 必须指向同一次构建的目录。
- 不启用 `OSG_MSVC_VERSIONED_DLL`。若开启，DLL 前缀实际是
  `osg${OPENSCENEGRAPH_SOVERSION}-` = `osg161-`，不是
  `osg3.6.5-`；`.lib` 名通常不变，只有 DLL 带前缀。

### 4.6 运行时布局

OSG 的构建树布局（由 `add_subdirectory` 的二进制目录决定）：

```text
build/3rdparty/osg/
├── src/                         OSG 源码子目录的构建目录
├── bin/
│   ├── osg.dll / osgDB.dll / osgGA.dll / OpenThreads.dll / ...
│   ├── osgversion.exe / osgconv.exe / osgviewer.exe / ...
│   └── osgPlugins-3.6.5/
│       ├── osgdb_png.dll
│       ├── osgdb_jpeg.dll
│       ├── osgdb_freetype.dll
│       └── ...
├── lib/                         OSG 导入库
│   └── osgPlugins-3.6.5/        插件的导入库（排障用）
└── include/                     OSG 生成的 Config/Version 头
```

`add_subdirectory` 的二进制目录是 `build/3rdparty/osg`，`OSG_BINARY_DIR`
就指向这里；上面的 `bin/lib/include` 由 OSG 的 `PROJECT_BINARY_DIR` 派生。

运行时需要：

```text
PATH           += <OSG_RUNTIME_DIR>
OSG_LIBRARY_PATH = <OSG_RUNTIME_DIR>
```

`OSG_LIBRARY_PATH` 指向 `bin`，OSG 自己在后面拼
`osgPlugins-3.6.5/`。

实测补充：MSVC 下 OSG 的插件 DLL 默认落在各自的源码子目录
（如 `src/osgPlugins/osg/osgdb_osg.dll`），**不会**自动出现在
`bin/osgPlugins-3.6.5/`。`examples/osg_demo` 因此用一个
`osg_demo_plugins` 自定义目标，只收集示例需要的插件并复制到
`OSG_PLUGIN_DIR`；`cmake --install` 不在本期发布路径内。

### 4.7 部署与安装

本期推荐：

- OSG 不进入 `demo_app` 的默认安装包；
- 新增一个可选的 `deploy_osg_runtime` 目标，把 `OSG_RUNTIME_DIR/*.dll`
  与 `osgPlugins-3.6.5/` 复制到 `build/bin/` 和
  `build/bin/osgPlugins-3.6.5/`；
- 给 OSG 示例单独生成 run 脚本，设置 `PATH` 与 `OSG_LIBRARY_PATH`；
- 将来某个应用真正链接 OSG 时，再让该应用的 `make_deploy` / `make_package`
  依赖这个目标，并把 OSG 许可证加入安装包。

这样不会让不关心 3D 的示例和安装包背上几十 MB 的 OSG 运行时。

### 4.8 Qt 与 GLCORE 的编译单元约束

这是使用 OSG 的硬约束，接入层只能提供文档和示例，不能靠 include 顺序
绕过：

- 包含 `QOpenGLWidget` / `QOpenGLContext` 等 Qt GL 头的 `.cpp`，不得再包含
  任何 OSG 头；
- 包含 `osg/...` 或 `osgViewer/...` 的 `.cpp`，不得包含 Qt GL 头；
- 两侧通过一个只依赖 QtCore 类型和不透明句柄的接口通信；
- 需要 Qt 侧调用 GL 的地方，单独放进只包含 Qt 头的
  `qt_gl_bridge.cpp`。

建议在 `examples/osg_demo` 里按实验工程的 `osg_view` /
`osg_view_widget` / `osg_renderer` 三层拆分，作为本仓的参考实现。不要
把 OSG 头放进 `QOpenGLWidget` 子类的头文件里。

### 4.9 许可证

| 组件 | 许可证 | 处理 |
| --- | --- | --- |
| OpenSceneGraph | OSGPL 1.0（LGPL-2.1 系） | 动态链接；随运行时附带许可证文本 |
| zlib | zlib License | 现有依赖，补登记 |
| libpng | libpng License | 补登记 |
| libjpeg-turbo | BSD-3-Clause / IJG | 补登记 |
| freetype | FTL / GPLv2 双许可 | 选 FTL；补登记 |
| glcore 头 | MIT（Khronos） | 保留头文件版权声明 |

OSG 采用动态库形态，符合 OSGPL 对“可替换库”的预期；如果将来改成静态
链接 OSG，需要重新评估许可证义务。

---

## 5. 实施阶段

| 阶段 | 内容 | 验收 |
| --- | --- | --- |
| P0 | 归档源码：把 4 个 tar 包、glcore 头入库；更新 `.gitignore`、`DEPENDENCY_ARCHIVES`、补丁注册与 `git add` | 4 个 tar 解压目录 + `3rdparty/glcore/` 就位；`git status` 只出现预期文件 |
| P1 | 写 `cmake/osg/osg.cmake`，先只加依赖子目录与变量注入；选项/缓存变量全部 `CACHE ... FORCE` | configure 通过；zlib/png/jpeg/freetype 目标存在；缓存里 `OPENGL_PROFILE=GLCORE` |
| P2 | 接入 OSG，x64 RelWithDebInfo 构建 `osgversion` / `osgconv` | `osgversion` 输出 3.6.5；`dumpbin /headers` 为 x64 |
| P3 | x86 RelWithDebInfo 构建与验证 | `dumpbin` 为 x86；`osgconv` 能读取一个测试模型并输出 `.osgb` |
| P4 | 增加 `examples/osg_demo`（QOpenGLWidget + GLCORE + 分编译单元）与 run 脚本 | 能创建 3.3 Core 上下文、显示模型；x86/x64 各一张截图 |
| P5 | 文档、许可证、清理脚本、回滚验证 | 现有 `ctest` 全过；`demo_app` 构建不受影响；`clean_deps` 能删新解压目录 |

P2/P3 的构建建议先显式打开 `BUILD_OSG` 配置一次，再用 `-j 2` 单独构建
`osgversion`，避免一次全量构建把内存打满。`build_ninja` 脚本本身不传
`-D`，所以首次配置可以直接用 CMake，或在 `local.cmake` 里设
`BUILD_OSG ON`：

```bat
cmake -S . -B build\ninja-relwithdebinfo-x64 -DBUILD_OSG=ON
scripts\build_ninja.bat -t relwithdebinfo -a x64 -j 2 -m osgversion
```

若构建目录已经配置过，缓存里的 `BUILD_OSG=ON` 会保留，直接跑第二条即可。

---

## 6. 验证清单

### 6.1 构建

- [x] x64 RelWithDebInfo configure + build 通过
- [x] x86 RelWithDebInfo configure + build 通过
- [ ] Debug x64 至少 configure 通过，确认 `d` 后缀没有破坏目标名
- [x] `CMakeCache.txt` 中 `OPENGL_PROFILE=GLCORE`、`OSG_GL3_AVAILABLE=ON`
- [ ] `BUILD_OSG=OFF` 时原有 `demo_app`、示例、测试完全不受影响
- [x] 现有 ctest 全部通过（13/13；libjpeg 自带的测试已从 ctest 移除）
- [ ] 若执行 `cmake --install`，先确认没有悬空 include；本期不作为发布路径

### 6.2 OSG 本身

- [x] `osgversion` 输出 `OpenSceneGraph Library 3.6.5`（x64/x86 各一次）
- [x] `dumpbin /headers osg.dll` 的 machine 与目标架构一致（x64/x86）
- [x] `osgconv` 能读取测试模型并输出 `.osgb`（x64/x86 各一次）
- [x] `dumpbin /dependents osg.dll` 不出现 `zlib1.dll` 等外部 zlib
- [x] `osgPlugins-3.6.5` 下能看到 png/jpeg/freetype/deprecated/glsl 等插件

### 6.3 Qt/GLCORE

- [x] `QSurfaceFormat` 实际拿到 3.3 Core
- [x] 模型与贴图显示正常，不是黑屏/空场景
- [x] 同一编译单元没有同时包含 Qt GL 头与 OSG 头
- [x] 退出时无 `osg::NotifyHandler` 堆损坏（实验工程已踩过）
- [x] 截图或人工确认 GLCORE 渲染路径

### 6.4 仓库卫生

- [ ] `3rdparty/glcore` 被 git 跟踪
- [ ] 4 个 tar 包被 git 跟踪，解压目录仍被忽略
- [ ] 补丁在干净解压目录上可重放
- [x] `scripts/clean_deps.bat` 包含新增目录
- [x] 许可证表更新

---

## 7. 风险与对策

| 风险 | 影响 | 对策 |
| --- | --- | --- |
| `option()` / `set(CACHE)` 被普通变量顶掉 | GLCORE 静默变 GL2，构建目录被缓存污染 | 所有选项/缓存变量用 `CACHE ... FORCE`；必要时 `CMAKE_POLICY_DEFAULT_CMP0077=NEW`/`CMP0126=NEW` |
| Find 变量注入顺序错 | libpng 先 `find_package(ZLIB REQUIRED)`，配置失败或链系统 zlib | Find 变量块放在任何依赖 `add_subdirectory` 之前 |
| OSG 的 `Find*` 覆盖注入变量 | 配置到系统 zlib/png，或重复编译 | 先变量注入；configure 实测失败再打 `0002-in-tree-dependencies.patch` |
| 全核编译 OOM | 构建被系统终止 | 用 `build_ninja.* -j 2`；OSG 接入层不另设 job pool |
| RelWithDebInfo 产物带 `rd` | 部署脚本找不到 DLL | 四个 `CMAKE_*_POSTFIX` 缓存项在 OSG 前 FORCE；保存/恢复避免外溢 |
| Qt GL 头冲突 | 编译期 C4273/C2373 刷屏 | 强制分编译单元；示例给出三层拆分 |
| OSG install 规则外溢 | `cmake --install` 悬空 include 或污染 AppDir | 本期不把 OSG 纳入 install；不把 `CMAKE_SKIP_INSTALL_RULES` 当已验证机制 |
| 插件运行时找不到 | `readNodeFile` 返回空 | 设置 `OSG_LIBRARY_PATH`，部署时保留 `osgPlugins-3.6.5` 目录层级 |
| Debug/Release 混用 | 崩溃或加载失败 | 运行脚本绑定本次构建目录；文档强调同配置 |
| libjpeg-turbo SIMD 依赖 NASM | 新机器缺少 NASM 配置失败 | 默认 `WITH_SIMD=OFF`；需要时显式开启并检测 |
| libjpeg-turbo 工具无条件生成 | `build/bin` 多出 cjpeg/djpeg 等 | 依赖目录用 `EXCLUDE_FROM_ALL`；不要写不存在的 `WITH_TOOLS` |
| OSG 应用数量被低估 | 构建时间/OOM 高于预期 | `BUILD_OSG_APPLICATIONS=ON` 会编 6 个应用；只验收两个 |
| 可选依赖包名写错 | 系统库被意外探测 | 使用与 `find_package` 完全一致的 `GIFLIB`、`Poppler-glib`、`Lua52/Lua51` 等名称 |
| `OSG_3RDPARTY_DIR` 环境变量 | 覆盖注入的 freetype 路径 | `USE_3RDPARTY_BIN=OFF CACHE FORCE`，并提示/清理该环境变量 |
| 别名目标上设 include | CMake 报 alias 不能设属性 | 先设真实目标，再建 `OSG::` 别名 |
| OSGPL 许可 | 分发义务 | 动态链接；随运行时附许可证与版权文本 |

---

## 8. 决策记录

| ID | 决策 | 理由 |
| --- | --- | --- |
| D1 | 源码以 tar 包 + 补丁入库，解压目录不入库 | 与本仓现有依赖机制一致，版本边界清晰 |
| D2 | OSG 固定 GLCORE | 需求明确；避免 GL2/GLCORE 双份产物和头文件混用 |
| D3 | OSG 动态库，依赖静态库 | 保留插件机制，减少运行时 DLL，复用同一份 zlib |
| D4 | 复用 `zlibstatic`，不单独编 zlib | 消除重复 zlib，版本/编译选项一致 |
| D5 | 使用 `add_subdirectory`，不用 ExternalProject | 单次构建、目标可传递、贴近本仓现有依赖风格 |
| D6 | OSG 不进入 `cmake --install` 发布路径，只使用构建树 | Windows 打包走 Inno；不把 `CMAKE_SKIP_INSTALL_RULES` 当已验证机制 |
| D7 | 本期不把 OSG 链进 `demo_app` | 避免无谓的构建时间与安装包体积 |
| D8 | 本期只验证 Windows/MSVC | 实验数据完整；Linux/WSL 另立任务，避免一次改太多 |
| D9 | OSG 编译使用 `-j 2`，不单独设 job pool | 7 GB 内存下全核编译有 OOM 记录；并行度由构建脚本统一控制，保持 KISS |
| D10 | `BUILD_OSG` 默认 OFF，显式开启 | 避免每个现有构建目录都被 OSG 的解压、配置和编译拖慢；验证完成后再讨论默认值 |

---

## 9. 原待确认项（已按推荐执行）

1. **`BUILD_OSG` 默认值**：保持 `OFF`；在 `local.cmake` 或目标级显式开启。
   现有构建目录较多，默认 ON 会让每次 configure 都背 OSG 的解压、配置
   和编译成本。
2. **`osgversion` / `osgconv` 是否随构建**：推荐 `ON`，只用于验证，
   不进安装包。
3. **是否现在就做 `examples/osg_demo`**：推荐做，作为 Qt/GLCORE 分编译
   单元的参考；如果本期只想要库，可把 P4 延后。
4. **是否把 OSG 运行时打进主安装包**：推荐本期不打，等某个实际应用链接
   OSG 后再接部署与许可证。

以上四点确认后再进入 P0；确认前不改任何构建文件。

---

## 10. 实施记录（2026-11-05）

### 10.1 落地文件

| 文件 | 作用 |
| --- | --- |
| `cmake/osg/osg.cmake` | 依赖顺序、变量注入、OSG 选项、目标别名、运行目录 |
| `cmake/extract_dependencies.cmake` | `BUILD_OSG=ON` 时才解压 4 个 tar 包 |
| `cmake/patch_dependencies.cmake` | OSG 与 3 个依赖的补丁注册 |
| `patches/OpenSceneGraph/0001..0004` | CMake 最低版本、重复 uninstall、两处 C++17 兼容 |
| `patches/libpng/0001` | 去掉未使用的 ASM 语言 |
| `patches/libjpeg-turbo/0001..0003` | CMake 最低版本、重复 uninstall、跳过测试段 |
| `patches/freetype/0001` | CMake 最低版本 |
| `examples/osg_demo/` | 从 `qt-osg-base-env/blender-test` 移植的 Qt Widgets + OSG 查看器 |

`BUILD_OSG` 默认保持 `OFF`；默认构建不受 OSG 的解压、配置和编译影响。
CI 未开启 OSG，现有构建不受影响。

### 10.2 实施中发现并修掉的问题

1. **C++17 移除的旧 STL 设施**
   - `tristripper` 的 `std::mem_fun_ref`（补丁 0003）。
   - OBJ 插件的 `std::not1` / `std::ptr_fun`（补丁 0004）。
   这两处只在编译到对应目标时才暴露，OSG 3.6.5 自身仍按 C++11 编写。
2. **libpng 的 `project(LANGUAGES C ASM)`**
   没有 ASM 源；Ninja 只启用 C/CXX 时缺少 `CMAKE_ASM_COMPILE_OBJECT`，
   用 `libpng/0001` 去掉 `ASM`。
3. **重复的 `uninstall` 目标**
   OSG 和 libjpeg-turbo 都无条件 `add_custom_target(uninstall)`，同树构建
   报重复目标；分别打 `0002` 守卫。
4. **libpng/freetype 的 `install(EXPORT)`**
   要求 `zlibstatic` 进入导出集，而 `zlibstatic` 来自 crashpad 树内目标、
   不导出；只在 libpng/freetype 子树内设 `CMAKE_SKIP_INSTALL_RULES`，
   不作为发布机制。
5. **`CMAKE_INSTALL_DOCDIR` 空缓存**
   旧构建目录残留空值，libjpeg-turbo 的 `install(FILES ... DESTINATION "")`
   在 generate 阶段报错；进 libjpeg 前 `unset(... CACHE)`。
6. **freetype 的 PNG 探测**
   它虽然设置了 `FT_DISABLE_PNG`，但后面仍无条件 `if(PNG_FOUND)` 打开
   `FT_CONFIG_OPTION_USE_PNG`；而它拿到的 PNG 头路径只有源码树，
   缺构建目录生成的 `pnglibconf.h`。本仓关掉 freetype 的 PNG 支持，
   并在其子目录作用域内临时把 `PNG_FOUND` 置假。
7. **MSVC 插件输出位置**
   OSG 在 MSVC 下把插件 DLL 留在各自源码子目录，运行期却只查
   `OSG_LIBRARY_PATH/osgPlugins-3.6.5`。`osg_demo` 用
   `osg_demo_plugins` 目标收集所需插件。
8. **libjpeg-turbo 的测试污染宿主 ctest**
   它的测试段无条件 `enable_testing()` + `add_test()`，测试目标却因
   `EXCLUDE_FROM_ALL` 从不编译，导致 ctest 多出 115 条 `Not Run`。
   `libjpeg-turbo/0003` 把测试段整体跳过。
9. **`.osg` 旧格式需要 deprecated 插件**
   只复制 `osgdb_osg.dll` 时，`cow.osg` 报
   `Error reading file ...: read error (No data loaded)`，日志里同时出现
   `osgdb_deprecated_osg.dll` 加载失败。必须同时复制
   `osgdb_deprecated_osg.dll`；`osgdb_glsl.dll` 用于 osgText 的 shader
   回退路径；`.osgt/.osgb` 还需要 `osgdb_serializers_osg.dll`。

### 10.3 验证结果

x64 RelWithDebInfo：

- `osg.dll`、`osgDB.dll`、`osgUtil.dll`、`osgGA.dll`、`osgViewer.dll`、
  `osgText.dll` 均生成；缓存中 `OPENGL_PROFILE=GLCORE`、
  `OSG_GL3_AVAILABLE=ON`、`OSG_GL1/2_AVAILABLE=OFF`。
- `run_osg_demo.bat -m assets\models\cow.osg -s cow.png -d 2500`
  退出码 0，截图非黑，模型、纹理、坐标轴、标注可见。
- `run_osg_demo.bat -m assets\scenes\desktop_pc.json -s scene.png -d 3000`
  退出码 0，场景装配与 OBJ/MTL/贴图显示正常。
- 运行日志里有 GLCORE 下固定管线状态造成的
  `invalid enumerant` 提示，但不影响画面；这是旧 `.osg` 材质/纹理状态
  在 Core Profile 下的已知噪声。

x86 RelWithDebInfo：

- 构建通过；使用 `-j 5` 完成剩余目标。
- `cow.osg` 与 `desktop_pc.json` 两张截图均生成，退出码 0；插件目录
  包含 deprecated/glsl/serializers 等 9 个 DLL。
- 画面与 x64 一致，GLCORE 路径、模型、贴图、标注、坐标轴均正常。

### 10.4 尚未做

- 未把 OSG 链进 `demo_app`，未加入 Inno Setup 安装包。
- 未验证 Linux/WSL（按本期范围）。
- 未提交代码；提交前需要用户确认。
