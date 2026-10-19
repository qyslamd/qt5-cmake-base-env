# 独立的 Markdown 查看器：单篇 + 整个目录两种打开方式，渲染全交给
# src/markdown（MarkdownView），这里只做宿主策略。目标是"发给同事也能用"
# 的产品型示例，不是教学样板
set(TARGET_NAME "md_viewer")

set(ALL_SRCS
    main.cpp
    md_viewer_settings.cpp
    md_viewer_settings.h
    md_viewer_window.cpp
    md_viewer_window.h
    render_settings_panel.cpp
    render_settings_panel.h
    render_status_panel.cpp
    render_status_panel.h
    md_viewer.qrc
)

add_executable(${TARGET_NAME} WIN32 ${ALL_SRCS})

target_link_libraries(${TARGET_NAME} PRIVATE ${APP_TARGET_PREFIX}_markdown)

# 这个示例**固定走 fluent**：FluentUI3Style + Frameless（QWK 无边框壳），
# 不加载任何 QSS，也不跟 APP_UI_ROUTE 走——它就是这个路线的试验场
if(NOT BUILD_FLUENT_UI)
    message(FATAL_ERROR
        "md_viewer 固定走 fluent，需要 BUILD_FLUENT_UI=ON（见顶层构建开关区）")
endif()
target_link_libraries(${TARGET_NAME} PRIVATE
    FluentUI3Style
    Frameless
)
target_compile_definitions(${TARGET_NAME} PRIVATE APP_UI_FLUENT)

# 导出 PDF：QPrinter 归宿主建（模块只透传），所以链接在示例侧
target_link_libraries(${TARGET_NAME} PRIVATE Qt5::PrintSupport)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc /utf-8)
endif()

# Windows .rc：exe 的文件图标与版本信息（右键属性里那几行）。模板与 demo_app
# 共用；版本号走同一份 version_manage（git tag + 提交数），所以示例的 exe 属性
# 与主程序对得上同一个版本。APP_APP_NAME 只在本目录覆盖，不影响别的目标
#
# icon.ico 由 resources/svg/icon.svg 生成（换了图标重跑这行）：
#   magick -background none svg/icon.svg \
#       -define icon:auto-resize=256,128,64,48,32,16 icon.ico
if(WIN32 AND MSVC)
    include(${CMAKE_SOURCE_DIR}/cmake/version_manage.cmake)
    resolve_app_version(APP)
    set(APP_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico")
    set(APP_APP_NAME "Markdown 查看器")
    set(OUT_RC_FILE ${CMAKE_CURRENT_BINARY_DIR}/md_viewer.rc)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/template/app.rc.in
        ${OUT_RC_FILE}
        @ONLY)
    target_sources(${TARGET_NAME} PRIVATE ${OUT_RC_FILE})
endif()

# 生成运行脚本（Windows 为 .bat，Linux 为 .sh），输出到可执行文件所在目录
configure_run_script("run_${TARGET_NAME}")

# 部署：独立目标，不挂顶层的 make_deploy，也不参与 build_ninja.bat 的 -d/-p。
# md_viewer 有自己的打包脚本（见文件末尾），windeployqt 由它自己跑；保留这个
# 目标只是为了需要时手动执行 --target deploy_md_viewer
if(WIN32 AND EXISTS ${WINDEPLOYQT_EXECUTABLE})
    add_custom_target(deploy_${TARGET_NAME}
        COMMAND ${WINDEPLOYQT_EXECUTABLE}
            --verbose 1
            --no-angle
            --no-opengl-sw
            --no-system-d3d-compiler
            --compiler-runtime
            "$<TARGET_FILE:${TARGET_NAME}>"
        COMMENT "Running windeployqt for ${TARGET_NAME}..."
    )
    add_dependencies(deploy_${TARGET_NAME} ${TARGET_NAME})
endif()

#-------------------------------------------------------------------------------
# 独立 Windows 安装包：信息全部在本文件里设置，不取顶层项目定制区，也不参与
# 顶层 build_ninja.bat 的 -p / -d。
#
#   cmake --build <build_dir> --target package_md_viewer
#
# 也可以直接运行 <build_dir>/examples/md_viewer/make_win_install.bat。脚本先跑
# windeployqt，再调用 Inno Setup 生成安装包，输出到 dist/md_viewer/
#-------------------------------------------------------------------------------
if(WIN32)
    find_program(MD_VIEWER_WINDEPLOYQT
        NAMES windeployqt
        HINTS "${QT_BIN_DIR}"
        NO_DEFAULT_PATH
    )
    find_program(MD_VIEWER_ISCC
        NAMES ISCC
        HINTS "${CMAKE_SOURCE_DIR}/3rdparty/Inno Setup 6"
    )
    if(EXISTS "${MD_VIEWER_WINDEPLOYQT}" AND EXISTS "${MD_VIEWER_ISCC}")
        if(NOT COMMAND resolve_app_version)
            include(${CMAKE_SOURCE_DIR}/cmake/version_manage.cmake)
        endif()
        resolve_app_version(MD_VIEWER)

        # 产品信息：改动这里即可，不需要动顶层 CMakeLists.txt
        set(MD_VIEWER_PACKAGE_NAME "Markdown 查看器")
        set(MD_VIEWER_PACKAGE_PUBLISHER "zhouyonghua")
        set(MD_VIEWER_PACKAGE_URL "https://github.com/qyslamd/qt5-cmake-base-env")
        # 每个产品必须独立的 Inno AppId；此 GUID 仅属于 md_viewer
        set(MD_VIEWER_APP_ID "{{1ff7ae24-b77c-4d93-b454-85bc337dc159}}")
        set(MD_VIEWER_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico")
        set(MD_VIEWER_WHERE_ARE_FILES "${CMAKE_BINARY_DIR}/bin")
        set(MD_VIEWER_PACKAGE_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/dist/md_viewer")

        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(MD_VIEWER_ARCH_SUFFIX "x64")
            set(MD_VIEWER_USE_64BIT "YES")
            set(MD_VIEWER_VC_REDIST "vc_redist.x64.exe")
        else()
            set(MD_VIEWER_ARCH_SUFFIX "x86")
            set(MD_VIEWER_USE_64BIT "NO")
            set(MD_VIEWER_VC_REDIST "vc_redist.x86.exe")
        endif()

        set(MD_VIEWER_ISS_FILE "${CMAKE_CURRENT_BINARY_DIR}/md_viewer.iss")
        set(MD_VIEWER_INSTALLER_BAT
            "${CMAKE_CURRENT_BINARY_DIR}/make_win_install.bat")
        file(MAKE_DIRECTORY "${MD_VIEWER_PACKAGE_OUTPUT_DIR}")

        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/md_viewer.iss.in"
            "${MD_VIEWER_ISS_FILE}"
            @ONLY
            NEWLINE_STYLE DOS
        )
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/make_win_install.bat.in"
            "${MD_VIEWER_INSTALLER_BAT}"
            @ONLY
            NEWLINE_STYLE DOS
        )

        add_custom_target(package_${TARGET_NAME}
            COMMAND "${MD_VIEWER_INSTALLER_BAT}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            COMMENT "Packaging ${TARGET_NAME} with windeployqt + Inno Setup..."
        )
        add_dependencies(package_${TARGET_NAME} ${TARGET_NAME})
    else()
        message(STATUS
            "md_viewer: windeployqt or ISCC not found; package target skipped")
    endif()
endif()
