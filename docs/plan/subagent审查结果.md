审查对象：`D:\trae_workspace\qt-cmake-base-env\docs\plan\OSG-GLCORE源码集成方案.md`（下称"文档"）。
  核对范围：本仓顶层 `CMakeLists.txt`、`cmake/extract_dependencies.cmake`、`cmake/patch_dependencies.cmake`、`cmake/crashpad/crashpad.cmake`、`scripts/build_ninja.bat`、`CMakePresets.json`、`.gitignore`、现有构建目录；实验工程 `D:\trae_workspace\qt-osg-base-env\osg-compile-glcore` 的脚本、补丁与 OSG 3.6.5 / libpng 1.6.43 / libjpeg-turbo 2.1.4 / freetype 2.12.1 源码；本仓自带 CMake 4.2 的策略与 Find 模块文档。未修改任何文件。

  结论：存在 1 个 🔴 阻塞项（变量注入方式不成立，GLCORE 会静默变成 GL2），其余为风险与建议。未发现"构建必失败"级别的第二个阻塞项，但 install 屏蔽机制已被本仓实例证明失效，属高优先 🟡。

  ---

  ## 🔴 阻塞

  ### R1. §4.3 步骤 4–8 用"普通变量"配置子项目，会全部被子项目的 `option()` / `set(... CACHE ...)` 顶掉，最终构建出 GL2 而不是 GLCORE

  **位置**：文档 201–235 行（步骤 4–8）、258–296 行（§4.4 及其说明）、444 行（风险表"先变量注入"）。示例写法是 `set(OPENGL_PROFILE GLCORE)`、`set(OSG_MSVC_VERSIONED_DLL OFF)`、`set(PNG_SHARED OFF)`、`set(WITH_SIMD OFF)` 这类普通变量。

  **事实与证据**：
  - OSG 3.6.5 的 `CMakeLists.txt:460` 是 `SET(OPENGL_PROFILE "GL2" CACHE STRING ...)`；`:462-540` 再按 profile 派生 `OPTION(OSG_GL*_AVAILABLE ...)`；`:394` `OPTION(OSG_MSVC_VERSIONED_DLL ... ON)`；`:445` `OPTION(OSG_USE_UTF8_FILENAME ... OFF)`；`:505-507` 在 Windows 上 `OSG_TEXT_USE_FONTCONFIG` 默认 **ON**；`:716/719/722` 应用/示例/插件；`:1094` `DYNAMIC_OPENSCENEGRAPH`。
  - libpng 1.6.43 `CMakeLists.txt:73-75`：`PNG_SHARED`/`PNG_STATIC`/`PNG_TESTS` 默认 ON/ON/ON；libjpeg-turbo 2.1.4 `CMakeLists.txt:208`：`WITH_SIMD` 默认 TRUE，`ENABLE_SHARED` 默认 TRUE；freetype 2.12.1 `CMakeLists.txt:189-215`：`FT_DISABLE_*` 默认 OFF。
  - 四个子项目的 `cmake_minimum_required` 是 3.5/3.6，按 CMake 4.2 `Help/command/include/POLICY_VERSION.rst`，这会 unset 后续策略，因此 CMP0077/CMP0126 在其作用域内是 **OLD**：`option()` 会移除同名普通变量，`set(CACHE)` 也会让缓存值压过父作用域的普通变量（CMP0077.rst 的动机就是"嵌入子项目时硬编码选项"，CMP0126.rst 同样给了 `CMAKE_POLICY_DEFAULT_CMP0126` 这一子项目用法）。
  - 实验工程之所以能成功，是因为 `scripts/build_osg_glcore.bat:45-60` 全部用 `-D` **命令行缓存变量**，不是普通变量。这是本方案与已验证配方的关键差异。
  - 本仓对嵌入依赖的既有写法就是缓存 + FORCE：`CMakeLists.txt:234-237`（qlitemd）、`264-265`（spdlog）、`365-370`（QWindowKit）、`398-400`（SARibbon）、`416-418`（kissfft）。

  **后果**：
  - `OPENGL_PROFILE` 缓存里仍是 `GL2`，OSG 的 GLCORE 分支不执行，`osg/Config` 与插件按 GL2 生成；`OSG_GL3_AVAILABLE` 被缓存成 OFF 后，之后即使改对 profile，`option()` 也不会覆盖已有带类型缓存项，构建目录被永久污染。方案的核心目标（GLCORE）静默落空。
  - `OSG_MSVC_VERSIONED_DLL` 仍为 ON，实际前缀是 `osg161-`（`CMakeLists.txt:7` `OPENSCENEGRAPH_SOVERSION 161`，`OsgMacroUtils.cmake:515-543`），与文档 §4.5/§4.6 的 `osg.dll` 假设不符。
  - libpng 仍编共享库、测试与工具；libjpeg 仍开 SIMD（依赖 NASM；实验脚本注释与 `build_osg_glcore.bat` 都显式带 `-DWITH_SIMD=1`，说明它从未验证过"默认关 SIMD"）；freetype 的 `FT_DISABLE_*` 无效，会自动探测系统 bzip2/harfbuzz/brotli，静态 freetype 可能链上系统 DLL；`OSG_TEXT_USE_FONTCONFIG` 在 Windows 上仍为 ON，`src/osgText/CMakeLists.txt:1-2,53-56` 会尝试 `find_package(Fontconfig)`，开发机若装了 fontconfig，OSG 运行时就多一个未部署的 DLL 依赖。
  - 文档 444 行的兜底方案（"实测失败再打 `0002-in-tree-dependencies.patch`"）修的是 Find 模块，治不了 option/cache 覆盖问题。

  **修正**：
  1. 对所有子项目的 `option()` / `set(CACHE)` 变量统一用本仓既有写法，例如：
     `set(OPENGL_PROFILE GLCORE CACHE STRING "" FORCE)`、
     `set(OSG_MSVC_VERSIONED_DLL OFF CACHE BOOL "" FORCE)`、
     `set(OSG_USE_UTF8_FILENAME ON CACHE BOOL "" FORCE)`、
     `set(OSG_TEXT_USE_FONTCONFIG OFF CACHE BOOL "" FORCE)`、
     `set(PNG_SHARED OFF CACHE BOOL "" FORCE)`、
     `set(WITH_SIMD OFF CACHE BOOL "" FORCE)`、
     `set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)` 等，全部在对应 `add_subdirectory` 之前。
     或者在接入层开头设 `CMAKE_POLICY_DEFAULT_CMP0077=NEW`、`CMAKE_POLICY_DEFAULT_CMP0126=NEW`；前者能同时管住 `option()`，但 `FORCE` 更确定，也与仓库风格一致。
  2. 已配置过的构建目录里，`OSG_GL*_AVAILABLE` 等派生项可能已被缓存成 GL2 组合。需在 vcvars 环境下用 `cmake -B <构建目录> -U 'OSG_GL*' -U OPENGL_PROFILE -U OSG_MSVC_VERSIONED_DLL ...`，或删 `CMakeCache.txt`（注意 CLAUDE.md 规则 19/20 的缓存覆盖与注释删除注意事项）。
  3. §4.4 的 `ZLIB_/PNG_/JPEG_/FREETYPE_` **Find 变量本身不受影响，可以继续用普通变量**（`find_*` 在变量已定义时会跳过搜索：`Help/command/include/FIND_XXX.rst:36-46`；CMake 4.2 `FindPNG.cmake` 有 `if(NOT PNG_LIBRARY)` 保护，`FindZLIB`/`FindJPEG` 同理）。R1 只针对 option/cache 变量。

  ---

  ## 🟡 风险 / 需要确认

  ### Y1. `CMAKE_RELWITHDEBINFO_POSTFIX` 的普通变量同样无效，且缓存会外溢到本仓后续所有库目标

  **位置**：文档 231–235、301、446 行。
  **证据**：OSG `CMakeLists.txt:846-849` 用 `SET(CMAKE_DEBUG_POSTFIX "d" CACHE ...)`、`SET(CMAKE_RELWITHDEBINFO_POSTFIX "rd" Con-2.5.7 `CMakeLists.txt:93-96` 也写同名缓存项，但**没有 FORCE**。顶层顺序是 crashpad(280) → QWindowKit(344-380) →SARibbon(384-401) → FluentUIStyle(425-441) → `add_subdirectory(src)`(451)。若按文档"crashpad 之后"把 OSG 放在 282 行附近，OSG 会先写入 `rd`，SARibbon 的非 FORCE `set` 无法覆盖，RelWithDebInfo（本仓发布配置）下 OSG 与后续库全部带 `rd`；若放在 SARibbon 之后，仅在 `BUILD_RIBBON=OFF` 时才会出问题。文档 446 行的对策（"清空普通变量"）本身不成立。
  **补充**：可执行文件不受 `<CONFIG>_POSTFIX` 影响（CMake `Help/prop_tgt/CONFIG_POSTFIX.rst` 明确只初始化非可执行目标），已有 .exe` 无后缀而 `demo_cored.lib` 有后缀。不要把 exe 改名当风险。
  **修正**：在 OSG 接入块之前把四个缓存项显式 FORCE 成仓库期望值（`d` / `""` / `""` / `""`），或在 OSG 块之后恢复；不要依赖 SARibbon 的缓存项存在。

  ### Y2. `CMAKE_SKIP_INSTALL_RULES` 在本仓已被证明会留下悬空 `include()`，`cmake --install` 会失败

  **位置**：文档 236–238、448 行；文档 190 行还声称"所有依赖的 install 规则都屏蔽"，但步骤 4–6 对 libpng/libjpeg/freetype 根本没有做屏蔽，前后不一致。
  **证据**：`build/ninja-relwithdebinfo-x64/cmake_install.cmake:87` 与 `build/ninja-relwithdebinfo-x86/cmake_install.cmake:92` 都 include 了 `.../3rdparty/FluentUIStyle-4.0/cmake_install.cmake`，而该文件**不存在**（对应目录存在，`kissfft/SARibbon/QtTesting` 的同名文件都在）。这正是顶层 `CMakeLists.txt:439-441` 那段 `CMAKE_SKIP_INSTALL_RULES` 的产物：子目录的 install 生成器仍被登记，父脚本照样 include，但子脚本未生成。对 OSG 重复该写法，每个启用 OSG 的构建目录都会多一条悬空 include。
  **说明**：这不是编译阻塞（Windows 打包走 Inno Setup，不调 `cmake --install`；AppImage 的 `cmake/template/make_appimage.sh.in门跳过），但 D6"屏蔽 install"的前提不成立。
  **修正**：先加一个 `cmake --install <构建目录> --prefix <工作区 temp 目录>` 的冒烟测试。若确实需要隔离，建议建一个包装目录（只在该子树内）定义空 `function(install) endfunction()` 再 `add_subdirectory`，这样不会影响 `src` 的 install 规则；或者直接放弃 `cmake --install` 并删除 D6 的表述。不要把现有 FluentUI 写法当作已验证机制。

  ### Y3. libjpeg-turbo 2.1.4 没有 `WITH_TOOLS` 选项，工具会无条件生成并落到本仓 `build/.../bin`

  **位置**：文档 206 行。
  **证据**：`libjpeg-turbo-2.1.4/CMakeLists.txt:684-701` 无条件 `add_executable` 生成 `cjpeg-static`/`djpeg-static`/`jpegtran-static`/`rdjpgcom`/`wrjpgcom`；`WITH_TURBOJPEG` 打开时还生成 turbojpeg 测试 exe。顶层 `CMakeLists.txt:178-180` 的 `CMAKE_RUNTIME_OUTPUT_DIRECTORY` 会被依赖子目录继承，所以这些 exe 会进 `build/ninja-*/bin`。实验脚本里的 `-DWITH_TOOLS=0` 同样是个空操作（该版本没有这个变量），实验工程实际上也编了这些工具。
  **修正**：对四个依赖子目录用 `add_subdirectory(... EXCLUDE_FROM_ALL)`（静态库被 OSG 链接时仍会构建，工具/测试不构建），或给 受并在清理脚本里处理。

  ### Y4. 可选依赖的 `CMAKE_DISABLE_FIND_PACKAGE_*` 名字与实验工程不一致

  **位置**：文档 227–230 行。`GIF`→应为 `GIFLIB`，`Poppler`→`Poppler-glib`，`Lua`→`Lua52`/`Lua51`。`CMAKE_DISABLE_FIND_PACKAGE_<name>` 必须与 `find_package(<name>)` 完全同名，否则不生效。直接照抄 `build_osg_glcore.bat:61-89` 的完整名单即可。

  ### Y5. `OSG::` 别名上不能补 `INTERFACE` include 目录

  **位置**：文档 244–249 行。"给这些目标补 `INTERFACE` include 目录"如果写成 `target_include_directories(OSG::osg INTERFACE ...)`，CMake 会报 "Cannot specify include directories for target ... which is an alias"。必须先对真实目标
  `osg`、`osgDB`、`osgGA`、`osgUtil`、`osgViewer`、`osgText`、`OpenThreads` 设置 `target_include_directories(... INTERFACE ...ALIAS xxx)`。

  ### Y6. §4.4 的变量块必须放在步骤 4（libpng）之前，文档只说了"OSG 的 add_subdirectory 之前"

  **位置**：文档 258–259 行；步骤顺序是 4 libpng、5 libjpeg、6 freetype、7 OSG（201–230 行）。
  **证据**：libpng `CMakeLists.txt:99` 是 `find_package(ZLIB REQUIRED)`；freetype `CMakeLists.txt:259-269` 会 `find_package(PNG/ZLIB)`。若按文档字面只在 OSG 前设置，libpng 的 REQUIRED zlib 会先去找系统 zlib（Windows 上通常没有）导致 configure 失败，或链到系统 zlib。
  **修正**：在 `osg.cmake` 最前面（任何依赖 `add_subdirectory` 之前）集中设置 §4.4 全部变量。

  ### Y7. 补丁注册缺少可执行细节，且 4 个 tar 包当前未入库

  **位置**：文档 387 行 P0 只写"补丁注册"。
  **证据**：`cmake/patch_dependencies.cmake:28-80` 的 `apply_patch_to_dir()` 在源目录不存在时直接 FATAL，因此必须先往 `cmake/e5` 的 `DEPENDENCY_ARCHIVES` 加 4 个包，再在该文件的注册区写三条调用；顶层 `CMakeLists.txt:158-163`的解压/补丁顺序本身正确（在 crashpad 之前，早于 OSG 接入）。实验工程的三份补丁是 `--- a/CMakeLists.txt` / `+++ b/CMakeLists.txt` 形式，`git apply` 可从源目录重放。`git status` 显示四个 tar 包仍为 untracked，P0 需要 `git add`。

  ### Y8. `BUILD_OSG` 默认 ON 与"不破坏现有构建"冲突

  **位置**：文档 173、474–476 行。本仓已有 x64/x86 多份构建目录；默认 ON 会让下一次 configure 在每个目录里解压 4 个包、配置 4 个子项目并全量编译 OSG，configure 时间与内存占用都会明显上升。建议默认 OFF，在 `local.cmake` 或目标级显式开启，待验证完成后再改默认值（并注意 CLAUDE.md 规则 20：改 `option()` 默认值对已有构建目录无效，需 `-U BUILD_OSG`）。

  ### Y9. `BUILD_OSG_APPLICATIONS=ON` 实际会编 6 个应用，不是文档说的 2 个

  **位置**：文档 223、317 行。
  **证据**：`applications/CMakeLists.txt` 在 `DYNAMIC_OPENSCENEGRAPH` 下加入 `osgviewer`、`osgarchive`、`osgconv`、`osgfilecache`、`osgversion`、`present3D`。文档只提 `osgversion`/`osgconv`，会低估构建量与 OOM 风险（D9 的 `-j 2` 对策就是针对这个）。建议改文字并明确验收只针对其中两个。

  ### Y10. `USE_3RDPARTY_BIN=OFF` 的普通变量无效；默认 ON 在本布局里暂时无害，但 `OSG_3RDPARTY_DIR` 环境变量可以覆盖 §4.4 的注入

  **证据**：`CMakeModules/Find3rdPartyDependencies.cmake:115` 是 `OPTION(USE_3RDPARTY_BIN ... ON)`；`:128-149` 的搜索目录是 `${PARENT_DIR}/3rdparty` = 本仓 `3rdparty/3rdparty`（不存在），所以默认 ON 目前不会触发 `SEARCH_3RDPARTY`；但 `:131-134` 会读取环境变量 `OSG_3RDPARTY_DIR`，一旦设置且目录存在，就会执行 `:71-74` 的 `FREETYPE_INCLUDE_DIR_*` FORCE 等操作，覆盖 §4.4 的注入。
  **修正**：用 `set(USE_3RDPARTY_BIN OFF CACHE BOOL "" FORCE)`，并在接入层对 `OSG_3RDPARTY_DIR` 给出提示（或直接 `unset(ENV{OSG_3RDPARTY_DIR})`，但要谨慎影响用户环境）。

  ---

  ## 🔵 建议

  1. **B1**：文档 305 行的"`osg3.6.5-` 前缀"不准确；实际是 `osg${OPENSCENEGRAPH_SOVERSION}-` = **`osg161-`**（`CMakeLists.txt:7`、`OsgMacroUtils.cmake:515-543`）。另外 `.lib` 名不变，只有 DLL 名带前缀，"消费方链接名不一致"的说法过强。
  2. **B2**：`OPENGL_HEADER1` 在 OSG 的 GLCORE 分支里已由 OSG 自己设置（`CMakeLists.txt` 的 GLCORE 分支，`FIND_PACKAGE(GLCORE REQUIRED)` 之后），文档 216 行再传一次是冗余（无害）。
  3. **B3**：P0 验收"解压出 5 个源码目录"不准确：4 个 tar 解压目录 + 已入库、不参与解压的 `3rdparty/glcore/`。
  4. **B4**：§4.6 布局只画了 `bin/osgPlugins-3.6.5`；插件的 import lib 还会出现在 `lib/osgPlugins-3.6.5`（`SETUP_PLUGIN` 的 `SET_OUTPUT_DIR_PROPERTY_260`）。部署只拷 bin 没问题，但排障时要知道。
  5. **B5**：`DYNAMIC_OPENTHREADS` 默认 ON，会产出 `OpenThreads.dll`；§4.6 未列出，`deploy_osg_runtime` 的 `*.dll` 通配能覆盖  OpenThreads，正确。
  6. **B6**：`OSG_RUNTIME_DIR`/`OSG_PLUGIN_DIR` 目前靠 `include(osg.cmake)` 留在顶层作用域，后续 `src`/`examples` 可见；如果以后改成 `function()` 包装会丢，建议加注释或改用 `CACHE INTERNAL`（KISS 优先，现有写法即可）。

  ---

  ## 已核实成立的点（避免后续重复讨论）

  - **zlib 复用时机与作用域正确**：`cmake/crashpad/crashpad.cmake:57-69` 在 `if(NOT TARGET ZLIB::ZLIB)` 下 `add_subdirectory(3rdparty/zlib-v1.2.11 build/crashpad/zlib EXCLUDE_FROM_ALL)`，并给 `zlibstatic` 加了源目录 + `build/crashpad/zlib` 两个 INTERFACE include，覆盖生成的
  `zconf.h`；OSG 放在 crashpad 之后是必须的，文档判对了。
  - **§4.4 的 Find 变量注入机制成立**（前提是普通变量，且在任何依赖 `add_subdirectory` 之前设置）：`find_*` 遇到已定义变量会跳过搜索；`FindPNG` 有 `if(NOT PNG_LIBRARY)` 保护，`FindZLIB`/`FindJPEG` 同理；`PNG_INCLUDE_DIRS` 由 `PNG_PNG_INCLUDE_DIR` + `ZLIB_INCLUDE_DIR` 拼出，`PNG_LIBRARIES` 被重算为 `png_static;zlibstatic`，与文档写法等价。`ZLIB_INCLUDE_DIR` 指向源码目录本身不含 `zconf.h`，但 osgDB/插件会链接 `zlibstatic` 目标，构建目录的 include 通过 usage requirements 传播，可用；只是这种"靠链接目标兜底"的方式比较脆，建议在文档里点明。
  - **`OSG_LIBRARY_PATH` 语义正确**：`src/osgDB/Registry.cpp:553-570` 读取该变量，`:784` 按 `osgPlugins-<version>/osgdb_<ext>< `bin`，OSG 自己拼 `osgPlugins-3.6.5/`"的描述准确。
  - **插件输出路径正确**：`SETUP_PLUGIN` 输出到 `${OSG_PLUGINS}` = `bin/osgPlugins-3.6.5`；`osgversion`/`osgconv` 由 `SETUP_EXE` 输出到 `bin`。§4.6 布局与事实一致。
  - **不设 job pool、用 `-j 2` 的判断成立**：`scripts/build_ninja.bat:72,159,192-195` 已支持 `-j` 并转成 `--parallel`；D9 不需要额外实现。
  - **`.gitignore` 只需 `!3rdparty/glcore/`**：现有 `3rdparty/*/`（275 行）与 `!3rdparty/QCustomPlot/`（282 行）同型；对目录开会被其他规则误伤。
  - **补丁可用**：实验工程三份补丁均为 `a/CMakeLists.txt` 形式，`apply_patch_to_dir` 的 `git -c core.autocrlf=false apply --ignore-whitespace` 可从源目录重放；libpng 1.6.43 最低版本已是 3.6，不需要补丁；OSG `examples/` 的旧 `cmake_minimum_required` 因 `BUILD_OSG_EXAMPLES=OFF` 不会被引入，补丁到 3.5 后 CMake 4.2 可直接接受，不需要 `CMAKE_POLICY_VERSION_MINIMUM=3.5`。
  - **四个 tar 包目录名与解压目录名一致**：OSG 为 `OpenSceneGraph-OpenSceneGraph-3.6.5/`，libpng/libjpeg/freetype 同名，`extract_if_needed` 推导无误。
  - **共享 OSG + 静态依赖没有 CRT 隐患**：`CMakePresets.json` 的 Debug 是 `MultiThreadedDebugDLL`、RelWithDebInfo 是 `MultiThreadedDLL`，与 3.5 最低版本子项目在 CMP0091 OLD 下使用的默认 `/MDd`、`/MD` 一致，不会出现 LNK2038 `RuntimeLibrary` 不匹配。
  - **x86/x64**：实验工程按同一套参数做 x86/x64；`glcore/` 头与架构无关，未发现架构相关阻塞。

  未修改任何文件。