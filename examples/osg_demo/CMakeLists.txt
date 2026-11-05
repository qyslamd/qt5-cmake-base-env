set(TARGET_NAME osg_demo)

add_executable(${TARGET_NAME} WIN32
    src/app_res.qrc
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
    src/logger.cpp
    src/logger.h
    src/osg_widget.cpp
    src/osg_widget.h
    src/osg_view.h
    src/osg_renderer.cpp
    src/osg_renderer.h
    src/osg_view_widget.cpp
    src/osg_view_widget.h
    src/scene_assembler.cpp
    src/scene_assembler.h
    src/scene_tree_model.cpp
    src/scene_tree_model.h
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/osg_demo")

if(MSVC)
    target_link_options(${TARGET_NAME} PRIVATE /ENTRY:mainCRTStartup)
    target_compile_options(${TARGET_NAME} PRIVATE /utf-8)
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt5::Widgets
    OSG::osgViewer
    OSG::osgDB
    OSG::osgGA
    OSG::osgUtil
    OSG::osgText
    OSG::osg
    OSG::OpenThreads
)

# 资产与模型必须和 exe 保持 assets/scenes、output/parts 的相对布局，
# scene JSON 里的 ../../output/parts/... 不需要改写。
set(OSG_DEMO_RUNTIME_DIR "${CMAKE_BINARY_DIR}/bin/osg_demo")
file(MAKE_DIRECTORY "${OSG_DEMO_RUNTIME_DIR}")
add_custom_target(copy_osg_demo_assets ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory
            "${OSG_DEMO_RUNTIME_DIR}/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "${OSG_DEMO_RUNTIME_DIR}/assets"
    COMMAND ${CMAKE_COMMAND} -E remove_directory
            "${OSG_DEMO_RUNTIME_DIR}/output/parts"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/output/parts"
            "${OSG_DEMO_RUNTIME_DIR}/output/parts"
    COMMAND ${CMAKE_COMMAND} -E remove_directory
            "${OSG_DEMO_RUNTIME_DIR}/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            "${OSG_DEMO_RUNTIME_DIR}/shaders"
    COMMENT "Copying OSG demo assets"
)
add_dependencies(${TARGET_NAME} copy_osg_demo_assets)

# MSVC 下 OSG 插件 DLL 留在各自的源码子目录，而 osgDB 只在
# OSG_LIBRARY_PATH/osgPlugins-<version> 下查找。示例只收集自己需要的插件，
# 不依赖 cmake --install 的安装树。
set(_osg_demo_plugin_targets
    osgdb_serializers_osg
    osgdb_deprecated_osg
    osgdb_osg
    osgdb_obj
    osgdb_png
    osgdb_jpeg
    osgdb_rgb
    osgdb_freetype
    osgdb_glsl)
set(_osg_demo_plugin_files "")
set(_osg_demo_plugin_deps "")
foreach(_plugin IN LISTS _osg_demo_plugin_targets)
  if(TARGET ${_plugin})
    list(APPEND _osg_demo_plugin_files "$<TARGET_FILE:${_plugin}>")
    list(APPEND _osg_demo_plugin_deps ${_plugin})
  endif()
endforeach()
if(_osg_demo_plugin_deps)
  add_custom_target(osg_demo_plugins
      COMMAND ${CMAKE_COMMAND} -E make_directory "${OSG_PLUGIN_DIR}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${_osg_demo_plugin_files} "${OSG_PLUGIN_DIR}"
      DEPENDS ${_osg_demo_plugin_deps}
      COMMENT "Collecting OSG plugins for osg_demo")
  add_dependencies(${TARGET_NAME} osg_demo_plugins)
endif()

# 运行脚本不复制 OSG DLL；PATH/OSG_LIBRARY_PATH 指向构建树里的 OSG。
file(TO_NATIVE_PATH "${OSG_RUNTIME_DIR}" OSG_DEMO_OSG_NATIVE)
file(TO_NATIVE_PATH "${QT_BIN_DIR}" OSG_DEMO_QT_BIN_NATIVE)
file(TO_NATIVE_PATH "${QT_PLUGINS_DIR}" OSG_DEMO_QT_PLUGINS_NATIVE)
file(TO_NATIVE_PATH "${OSG_DEMO_RUNTIME_DIR}" OSG_DEMO_RUN_DIR_NATIVE)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/run_app.bat.in"
    "${OSG_DEMO_RUNTIME_DIR}/run_osg_demo.bat"
    @ONLY)
