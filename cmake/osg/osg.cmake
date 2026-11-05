# cmake/osg/osg.cmake
#
# OpenSceneGraph 3.6.5 GLCORE，源码内构建。
#
# 依赖顺序：zlibstatic（crashpad 已建） -> libpng -> libjpeg-turbo ->
# freetype -> OpenSceneGraph。OSG 3.6.5 仍使用旧的 find_package 变量接口，
# 这里把变量指向树内目标；所有 option()/set(CACHE) 项必须 CACHE FORCE，
# 否则子项目的策略 OLD 会忽略父作用域的普通变量。

if(NOT WIN32)
    message(STATUS "BUILD_OSG: 当前只支持 Windows，跳过")
    return()
endif()

set(_osg_3rdparty "${CMAKE_SOURCE_DIR}/3rdparty")
set(_osg_src "${_osg_3rdparty}/OpenSceneGraph-OpenSceneGraph-3.6.5")
set(_osg_bin "${CMAKE_BINARY_DIR}/3rdparty/osg")
set(_osg_deps_bin "${CMAKE_BINARY_DIR}/3rdparty/osg-deps")
set(_osg_glcore "${_osg_3rdparty}/glcore")

foreach(_required_dir
        "${_osg_src}"
        "${_osg_3rdparty}/libpng-1.6.43"
        "${_osg_3rdparty}/libjpeg-turbo-2.1.4"
        "${_osg_3rdparty}/freetype-2.12.1"
        "${_osg_glcore}")
    if(NOT EXISTS "${_required_dir}")
        message(FATAL_ERROR
            "OSG 源码未解压：${_required_dir}\n"
            "请先在 BUILD_OSG=ON 下重新 configure，让 "
            "cmake/extract_dependencies.cmake 解压依赖。")
    endif()
endforeach()

# 只认 crashpad 引入的真实目标。系统可能已经提供 ZLIB::ZLIB 别名，
# 但那种情况下没有 zlibstatic，不能让 OSG 静默链到系统 zlib。
if(NOT TARGET zlibstatic)
    message(FATAL_ERROR
        "找不到树内 zlibstatic。请确认 crashpad 的 zlib 引入先于 OSG，"
        "且构建目录里没有把 ZLIB::ZLIB 缓存成系统目标。")
endif()

# OSG 的 install 规则本期不进入发布路径；Windows 打包走 Inno Setup，
# 不调用 cmake --install。
set(_osg_saved_debug_postfix "${CMAKE_DEBUG_POSTFIX}")
set(_osg_saved_release_postfix "${CMAKE_RELEASE_POSTFIX}")
set(_osg_saved_relwithdebinfo_postfix "${CMAKE_RELWITHDEBINFO_POSTFIX}")
set(_osg_saved_minsizerel_postfix "${CMAKE_MINSIZEREL_POSTFIX}")
if(DEFINED CMAKE_DEBUG_POSTFIX)
    set(_osg_had_debug_postfix TRUE)
else()
    set(_osg_had_debug_postfix FALSE)
endif()
if(DEFINED CMAKE_RELEASE_POSTFIX)
    set(_osg_had_release_postfix TRUE)
else()
    set(_osg_had_release_postfix FALSE)
endif()
if(DEFINED CMAKE_RELWITHDEBINFO_POSTFIX)
    set(_osg_had_relwithdebinfo_postfix TRUE)
else()
    set(_osg_had_relwithdebinfo_postfix FALSE)
endif()
if(DEFINED CMAKE_MINSIZEREL_POSTFIX)
    set(_osg_had_minsizerel_postfix TRUE)
else()
    set(_osg_had_minsizerel_postfix FALSE)
endif()

set(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "" FORCE)
set(CMAKE_RELEASE_POSTFIX "" CACHE STRING "" FORCE)
set(CMAKE_RELWITHDEBINFO_POSTFIX "" CACHE STRING "" FORCE)
set(CMAKE_MINSIZEREL_POSTFIX "" CACHE STRING "" FORCE)

set(_osg_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
if(DEFINED BUILD_SHARED_LIBS)
    set(_osg_had_build_shared_libs TRUE)
else()
    set(_osg_had_build_shared_libs FALSE)
endif()
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(_osg_saved_ipo "${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")
if(DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    set(_osg_had_ipo TRUE)
else()
    set(_osg_had_ipo FALSE)
endif()
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)

# 依赖的 Find 变量：这些不是 option/cache，普通变量即可。
# 没有 zconf.h/jconfig.h/ftconfig.h 的构建目录在后面统一加入 include。
set(ZLIB_FOUND TRUE)
set(ZLIB_LIBRARY zlibstatic)
set(ZLIB_LIBRARIES zlibstatic)
# 这些是 Find 模块的缓存项；旧构建目录可能留下空的 NOTFOUND 缓存，
# 单靠普通变量不保证覆盖，这里直接 FORCE 到树内路径。
set(ZLIB_INCLUDE_DIR "${_osg_3rdparty}/zlib-v1.2.11"
    CACHE PATH "" FORCE)
set(ZLIB_INCLUDE_DIRS
    "${_osg_3rdparty}/zlib-v1.2.11;${CMAKE_BINARY_DIR}/crashpad/zlib")

set(PNG_FOUND TRUE)
set(PNG_LIBRARY png_static)
set(PNG_LIBRARIES "png_static;zlibstatic")
set(PNG_PNG_INCLUDE_DIR "${_osg_3rdparty}/libpng-1.6.43"
    CACHE PATH "" FORCE)
set(PNG_INCLUDE_DIR "${_osg_3rdparty}/libpng-1.6.43")
set(PNG_INCLUDE_DIRS
    "${_osg_3rdparty}/libpng-1.6.43;${_osg_deps_bin}/libpng")

set(JPEG_FOUND TRUE)
set(JPEG_LIBRARY jpeg-static)
set(JPEG_LIBRARIES jpeg-static)
set(JPEG_INCLUDE_DIR "${_osg_3rdparty}/libjpeg-turbo-2.1.4"
    CACHE PATH "" FORCE)
set(JPEG_INCLUDE_DIRS
    "${_osg_3rdparty}/libjpeg-turbo-2.1.4;${_osg_deps_bin}/libjpeg")

set(FREETYPE_FOUND TRUE)
set(FREETYPE_LIBRARY freetype)
set(FREETYPE_LIBRARIES freetype)
set(FREETYPE_INCLUDE_DIR_ft2build
    "${_osg_3rdparty}/freetype-2.12.1/include" CACHE PATH "" FORCE)
set(FREETYPE_INCLUDE_DIR_freetype2
    "${_osg_3rdparty}/freetype-2.12.1/include" CACHE PATH "" FORCE)
set(FREETYPE_INCLUDE_DIRS
    "${_osg_3rdparty}/freetype-2.12.1/include;${_osg_deps_bin}/freetype/include")

# libpng：静态库、无测试/工具。
set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_STATIC ON CACHE BOOL "" FORCE)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
set(PNG_FRAMEWORK OFF CACHE BOOL "" FORCE)
# libpng/freetype 的 install(EXPORT) 要求 zlibstatic 进入它们的 export set，
# 但 zlibstatic 是 crashpad 树内目标、不导出。本仓不使用 OSG 依赖的 install，
# 只在该子树内跳过 install 规则以通过 generate。
set(CMAKE_SKIP_INSTALL_RULES TRUE)
add_subdirectory("${_osg_3rdparty}/libpng-1.6.43"
                 "${_osg_deps_bin}/libpng"
                 EXCLUDE_FROM_ALL)
unset(CMAKE_SKIP_INSTALL_RULES)

# libjpeg-turbo：静态库；2.1.4 没有 WITH_TOOLS，工具靠 EXCLUDE_FROM_ALL 排除。
# 旧构建目录可能残留空的 CMAKE_INSTALL_DOCDIR；libjpeg 的 GNUInstallDirs
# 不会再覆盖空缓存，导致 install(FILES ... DESTINATION "") 配置失败。
unset(CMAKE_INSTALL_DOCDIR CACHE)
set(ENABLE_SHARED OFF CACHE BOOL "" FORCE)
set(ENABLE_STATIC ON CACHE BOOL "" FORCE)
set(WITH_TURBOJPEG OFF CACHE BOOL "" FORCE)
set(WITH_SIMD OFF CACHE BOOL "" FORCE)
add_subdirectory("${_osg_3rdparty}/libjpeg-turbo-2.1.4"
                 "${_osg_deps_bin}/libjpeg"
                 EXCLUDE_FROM_ALL)

# freetype：静态库，关闭可选 bzip2/brotli/harfbuzz/png。
# 本仓只需要字体轮廓；freetype 的 CMake 在 PNG_FOUND 为真时会无条件启用
# FT_CONFIG_OPTION_USE_PNG，而它拿到的 PNG 头路径只有源码树，缺构建目录里
# 生成的 pnglibconf.h。关掉 PNG，同时临时清掉全局 PNG_FOUND，避免这条
# 无条件的 if(PNG_FOUND) 仍然生效。
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(_osg_saved_png_found "${PNG_FOUND}")
set(PNG_FOUND FALSE)
set(CMAKE_SKIP_INSTALL_RULES TRUE)
add_subdirectory("${_osg_3rdparty}/freetype-2.12.1"
                 "${_osg_deps_bin}/freetype"
                 EXCLUDE_FROM_ALL)
unset(CMAKE_SKIP_INSTALL_RULES)
set(PNG_FOUND "${_osg_saved_png_found}")

# 依赖生成的配置头（zconf.h/jconfig.h/ftconfig.h 等）在构建目录。
include_directories(
    "${CMAKE_BINARY_DIR}/crashpad/zlib"
    "${_osg_deps_bin}/libpng"
    "${_osg_deps_bin}/libjpeg"
    "${_osg_deps_bin}/freetype/include"
)

# OSG 的 option/cache 项：全部 FORCE。
set(OPENGL_PROFILE GLCORE CACHE STRING "" FORCE)
set(OPENGL_HEADER1 "#include <GL/glcorearb.h>" CACHE STRING "" FORCE)
set(GLCORE_ROOT "${_osg_glcore}" CACHE PATH "" FORCE)
set(DYNAMIC_OPENSCENEGRAPH ON CACHE BOOL "" FORCE)
set(OSG_MSVC_VERSIONED_DLL OFF CACHE BOOL "" FORCE)
set(OSG_USE_UTF8_FILENAME ON CACHE BOOL "" FORCE)
set(OSG_TEXT_USE_FONTCONFIG OFF CACHE BOOL "" FORCE)
set(BUILD_OSG_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_OSG_APPLICATIONS ON CACHE BOOL "" FORCE)
set(BUILD_OSG_PLUGINS ON CACHE BOOL "" FORCE)
set(BUILD_OSG_PACKAGES OFF CACHE BOOL "" FORCE)
set(USE_3RDPARTY_BIN OFF CACHE BOOL "" FORCE)

if(DEFINED ENV{OSG_3RDPARTY_DIR})
    message(STATUS
        "OSG: 忽略环境变量 OSG_3RDPARTY_DIR，使用树内依赖")
    unset(ENV{OSG_3RDPARTY_DIR})
endif()

foreach(_osg_disabled_dep
        FFmpeg GDAL CURL TIFF GIFLIB OpenEXR ilmbase COLLADA FBX Jasper
        OpenCascade LibVNCServer DCMTK GStreamer GLIB DirectShow SDL SDL2
        Poppler-glib RSVG GtkGl DirectInput NVTT ZeroConf LIBLAS Lua52
        Lua51 Inventor QuickTime)
    set(CMAKE_DISABLE_FIND_PACKAGE_${_osg_disabled_dep}
        TRUE CACHE BOOL "" FORCE)
endforeach()

add_subdirectory("${_osg_src}" "${_osg_bin}")

# 先给真实目标补 include，再建立别名；alias 上不能再设属性。
foreach(_osg_target
        osg osgDB osgGA osgUtil osgViewer osgText OpenThreads)
    if(TARGET ${_osg_target})
        target_include_directories(${_osg_target} INTERFACE
            "${_osg_src}/include"
            "${_osg_bin}/include"
            "${_osg_glcore}")
        add_library(OSG::${_osg_target} ALIAS ${_osg_target})
    endif()
endforeach()

set(OSG_SOURCE_DIR "${_osg_src}")
set(OSG_BINARY_DIR "${_osg_bin}")
set(OSG_RUNTIME_DIR "${_osg_bin}/bin")
set(OSG_PLUGIN_DIR "${OSG_RUNTIME_DIR}/osgPlugins-3.6.5")
set(OSG_LIBRARY_PATH "${OSG_RUNTIME_DIR}")

message(STATUS "OSG GLCORE 已引入：")
message(STATUS "  源码根    : ${OSG_SOURCE_DIR}")
message(STATUS "  构建根    : ${OSG_BINARY_DIR}")
message(STATUS "  运行目录  : ${OSG_RUNTIME_DIR}")
message(STATUS "  插件目录  : ${OSG_PLUGIN_DIR}")

# 恢复会被子项目改写的缓存项，避免影响后面的 SARibbon 等目标。
if(_osg_had_debug_postfix)
    set(CMAKE_DEBUG_POSTFIX "${_osg_saved_debug_postfix}"
        CACHE STRING "" FORCE)
else()
    unset(CMAKE_DEBUG_POSTFIX CACHE)
endif()
if(_osg_had_release_postfix)
    set(CMAKE_RELEASE_POSTFIX "${_osg_saved_release_postfix}"
        CACHE STRING "" FORCE)
else()
    unset(CMAKE_RELEASE_POSTFIX CACHE)
endif()
if(_osg_had_relwithdebinfo_postfix)
    set(CMAKE_RELWITHDEBINFO_POSTFIX "${_osg_saved_relwithdebinfo_postfix}"
        CACHE STRING "" FORCE)
else()
    unset(CMAKE_RELWITHDEBINFO_POSTFIX CACHE)
endif()
if(_osg_had_minsizerel_postfix)
    set(CMAKE_MINSIZEREL_POSTFIX "${_osg_saved_minsizerel_postfix}"
        CACHE STRING "" FORCE)
else()
    unset(CMAKE_MINSIZEREL_POSTFIX CACHE)
endif()
if(_osg_had_build_shared_libs)
    set(BUILD_SHARED_LIBS "${_osg_saved_build_shared_libs}"
        CACHE BOOL "" FORCE)
else()
    unset(BUILD_SHARED_LIBS CACHE)
endif()
if(_osg_had_ipo)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION "${_osg_saved_ipo}")
else()
    unset(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
endif()
