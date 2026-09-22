set(TARGET_NAME ${APP_TARGET_PREFIX}_app)

#=======================================
# 版本管理：由 git 派生（tag 提供 major.minor.patch，提交数提供 build 号），
# 同一提交在任何机器上构建得到同样的版本号
include(${CMAKE_SOURCE_DIR}/cmake/version_manage.cmake)
resolve_app_version(APP)

configure_file(${CMAKE_SOURCE_DIR}/cmake/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/AppVersion.h
    @ONLY)

# 组织名 / 应用名：与安装脚本共用同一来源（见顶层 CMakeLists.txt）
configure_file(${CMAKE_SOURCE_DIR}/cmake/app_info.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/AppInfo.h
    @ONLY)


set(TARGET_SRCS 
    main.cpp
    res.qrc
    mainwindow.cpp
    mainwindow.h
    guidance_app_flows.cpp
    guidance_app_flows.h
    log_window.cpp
    log_window.h
    dialogs/about_dialog.cpp
    dialogs/about_dialog.h
    dialogs/add_edit_user_dialog.cpp
    dialogs/add_edit_user_dialog.h
    dialogs/change_password_dialog.cpp
    dialogs/change_password_dialog.h
    dialogs/login_dialog.cpp
    dialogs/login_dialog.h
    dialogs/settings_dialog.cpp
    dialogs/settings_dialog.h
    dialogs/user_manage_dialog.cpp
    dialogs/user_manage_dialog.h
    log_filter_bar.cpp
    log_filter_bar.h
    log_output_panel.cpp
    log_output_panel.h
    )

# UI 测试录制/回放（QtTesting）：仅 BUILD_QT_TESTING 打开的构建包含这部分。
# 开关见顶层 CMakeLists.txt
if(BUILD_QT_TESTING)
    list(APPEND TARGET_SRCS
        qttesting/testing_support.cpp
        qttesting/testing_support.h
        qttesting/qttesting_xml.h
        )
endif()

add_executable(${TARGET_NAME} WIN32 ${TARGET_SRCS})

#=======================================
# 生成 Windows .rc 文件（版本信息嵌入 exe）
if(WIN32 AND MSVC)
    set(APP_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.ico")
    set(OUT_RC_FILE ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/app.rc.in
        ${OUT_RC_FILE}
        @ONLY)
    target_sources(${TARGET_NAME} PRIVATE ${OUT_RC_FILE})
endif()

target_include_directories(${TARGET_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(${TARGET_NAME} PRIVATE
    ${APP_TARGET_PREFIX}_guidance
    ${APP_TARGET_PREFIX}_core
    qt-libui::libui
    qcustomplot
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
)

# 编译期开关：main.cpp 据此决定是否接线 UI 测试（菜单与 --play）
if(BUILD_QT_TESTING)
    target_link_libraries(${TARGET_NAME} PRIVATE qttesting)
    target_compile_definitions(${TARGET_NAME} PRIVATE APP_ENABLE_QT_TESTING)
endif()

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# 编译选项与 core 保持一致：MSVC 默认只有 /W3，界面层同样需要 /W4
if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX- /EHsc)
endif()

# 生成运行脚本（Windows 为 .bat，Linux 为 .sh），输出到可执行文件所在目录
configure_run_script("run_${TARGET_NAME}")

#=======================================
# 拷贝运行时目录到输出目录（每次构建后同步）
# 拷贝配置
set(CONFIG_FILES_SOURCE_DIR "${CMAKE_SOURCE_DIR}/config/calibration/calibration_sample.ini")
set(CONFIG_FILES_OUTPUT_DIR "$<TARGET_FILE_DIR:${TARGET_NAME}>/calibration/calibration.ini")
if(EXISTS "${CONFIG_FILES_SOURCE_DIR}")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  
            "${CONFIG_FILES_SOURCE_DIR}" # file
            "${CONFIG_FILES_OUTPUT_DIR}" # destination.either file or directory
        COMMENT "Copying calibration file to output directory..."
    )
endif()


#=======================================
# 安装打包工具链
#   Windows：windeployqt + Inno Setup，产出安装程序
#   Linux：linuxdeploy + appimagetool，产出 AppImage
# 两侧同构——同样的输出目录 dist/、同样的符号归档位置 dist/symbols/
#=======================================
if(WIN32)
    # 查找 windeployqt —— QT_BIN_DIR 由顶层统一推导，此处不再重复
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS "${QT_BIN_DIR}"
        NO_DEFAULT_PATH
    )
    if(NOT EXISTS ${WINDEPLOYQT_EXECUTABLE})
        message(WARNING "windeployqt.exe not found; Qt DLLs will not be auto-deployed")
    endif()

    # 查找 ISCC (Inno Setup Compiler)
    find_program(ISCC_EXECUTABLE
        NAMES ISCC
        HINTS "${CMAKE_SOURCE_DIR}/3rdparty/Inno Setup 6"
    )
    if(NOT EXISTS ${ISCC_EXECUTABLE})
        message(WARNING "ISCC.exe not found; installer package target unavailable")
    endif()

    #=======================================
    # make_deploy target — 按需触发 windeployqt
    if(EXISTS ${WINDEPLOYQT_EXECUTABLE})
        add_custom_target(make_deploy
            COMMAND ${WINDEPLOYQT_EXECUTABLE}
                --verbose 1
                --no-angle
                --no-opengl-sw
                --no-system-d3d-compiler
                --compiler-runtime
                "$<TARGET_FILE:${TARGET_NAME}>"
            COMMENT "Running windeployqt..."
        )
        add_dependencies(make_deploy ${TARGET_NAME})
    endif()

    #=======================================
    # 架构变量（供 configure_file 注入到 .iss.in / .bat.in）
    # 产品标识取自顶层定制区的 APP_INSTALLER_ID（已按架构选好）
    set(ISCC_APP_ID "${APP_INSTALLER_ID}")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ISCC_APP_PF "{autopf64}")
        set(ISCC_VC_REDIST "vc_redist.x64.exe")
        set(ISCC_USE_64BIT "YES")
    else()
        set(ISCC_APP_PF "{autopf}")
        set(ISCC_VC_REDIST "vc_redist.x86.exe")
        set(ISCC_USE_64BIT "NO")
    endif()
    set(APP_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.ico")

    set(PACKAGE_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/dist")
    set(ISCC_WHERE_ARE_FILES "${CMAKE_BINARY_DIR}/bin")

    # 生成打包脚本（可独立编译的 .iss + CLI .bat）
    set(ISS_FILE "${CMAKE_SOURCE_DIR}/scripts/make_install_package.iss")
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/make_install_package.iss.in"
        ${ISS_FILE}
        @ONLY
    )
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/make_installer.bat.in"
        "${CMAKE_SOURCE_DIR}/scripts/make_installer.bat"
        @ONLY
    )

    #=======================================
    # make_symbols target — 归档发布符号
    # 崩溃转储与崩溃日志里只有地址，事后解析要拿崩溃那一刻那个二进制匹配的
    # PDB。而 exe 内嵌的是构建机的绝对 PDB 路径（分析机上不存在），一旦被
    # 后续构建覆盖便再也拿不回来。
    # 目录名带上 git 短 hash：工作区有未提交改动时版本号不变但二进制不同，
    # 只按版本号归档会让脏构建覆盖掉发布构建的符号。
    # release 不产 PDB，由脚本告警而非中断打包。
    set(SYMBOLS_OUTPUT_DIR
        "${PACKAGE_OUTPUT_DIR}/symbols/${APP_VERSION}-${APP_GIT_HASH}-${APP_ARCH_SUFFIX}")
    if(MSVC)
        add_custom_target(make_symbols
            COMMAND ${CMAKE_COMMAND}
                -DSRC_EXE="$<TARGET_FILE:${TARGET_NAME}>"
                -DSRC_PDB="$<TARGET_PDB_FILE:${TARGET_NAME}>"
                -DDEST_DIR="${SYMBOLS_OUTPUT_DIR}"
                -P "${CMAKE_SOURCE_DIR}/cmake/archive_symbols.cmake"
            COMMENT "Archiving symbols to ${SYMBOLS_OUTPUT_DIR}..."
        )
        add_dependencies(make_symbols ${TARGET_NAME})
    endif()

    # make_package target — 调用生成的 .bat 完成 ISCC 打包。
    # 该 .bat 只负责 ISCC，windeployqt 必须先行，否则 .iss 引用的
    # Qt5*.dll 等运行时文件不存在会导致打包失败，故通过目标依赖串起来
    # （而非依赖调用方先手动跑 make_deploy）。符号归档同样是发布必经步骤。
    if(WIN32 AND EXISTS ${ISCC_EXECUTABLE})
        add_custom_target(make_package
            # 先校验构建配置：Debug 依赖调试版 C 运行时，打出的包在用户机器
            # 上无法启动，直接拒绝；非 relwithdebinfo 则告警
            COMMAND ${CMAKE_COMMAND}
                -DBUILD_TYPE="$<CONFIG>"
                -P "${CMAKE_SOURCE_DIR}/cmake/check_package_config.cmake"
            COMMAND "${CMAKE_SOURCE_DIR}/scripts/make_installer.bat"
                "$<TARGET_FILE_DIR:${TARGET_NAME}>"
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:${TARGET_NAME}>"
            COMMENT "Building installer package with Inno Setup (${APP_ARCH_SUFFIX})..."
        )
        add_dependencies(make_package ${TARGET_NAME})
        # windeployqt 未找到时 make_deploy 不存在，此时保持原有行为
        if(TARGET make_deploy)
            add_dependencies(make_package make_deploy)
        endif()
        if(TARGET make_symbols)
            add_dependencies(make_package make_symbols)
        endif()
    endif()
else() # 非 Windows：Linux 侧用 AppImage
    set(PACKAGE_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/dist")
    # 三个打包工具随仓库提供，理由见 3rdparty/appimage-tools/README.md
    set(APPIMAGE_TOOLS_DIR "${CMAKE_SOURCE_DIR}/3rdparty/appimage-tools")
    set(APPIMAGE_WORK_DIR "${CMAKE_BINARY_DIR}/appimage")
    set(APPIMAGE_ICON_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.png")
    set(APPIMAGE_DESKTOP_FILE "${APPIMAGE_WORK_DIR}/${APP_APP_NAME}.desktop")
    # AppImage 的架构名要用 appimagetool 认识的写法（x86_64 / i686 / aarch64 /
    # armhf），而 CMAKE_SYSTEM_PROCESSOR 在部分工具链下是 AMD64、arm64 之类，
    # 故做一次映射。该值同时决定工具文件名（linuxdeploy-${APPIMAGE_ARCH}.AppImage），
    # 使架构不一致时能被前置检查直接拦下
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64|x64)$")
        set(APPIMAGE_ARCH "x86_64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(APPIMAGE_ARCH "aarch64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3-6]86|x86)$")
        set(APPIMAGE_ARCH "i686")
    else()
        set(APPIMAGE_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
        message(WARNING
            "未识别的架构 ${CMAKE_SYSTEM_PROCESSOR}，AppImage 将原样使用该名；"
            "若 appimagetool 不认，请在顶层定制区之外自行映射")
    endif()

    # linuxdeploy 的 qt 插件靠 qmake 判断该打包哪套 Qt，必须与工程链接的是
    # 同一套，否则可能打进系统里另一份 Qt。取 Qt5 包提供的 qmake 烘焙进脚本
    if(TARGET Qt5::qmake)
        get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
    endif()
    if(NOT QT_QMAKE_EXECUTABLE OR NOT EXISTS "${QT_QMAKE_EXECUTABLE}")
        set(QT_QMAKE_EXECUTABLE "")
        message(WARNING
            "未定位到 qmake，打包脚本将交由 linuxdeploy 自行从 PATH 查找；"
            "多套 Qt 共存时可能打进错误的一套，可导出 QMAKE 指定")
    endif()

    set(APPIMAGE_CHECK_SCRIPT
        "${CMAKE_SOURCE_DIR}/cmake/check_package_config.cmake")
    set(APPIMAGE_BUILD_DIR "${CMAKE_BINARY_DIR}")
    set(APPIMAGE_OUTPUT_DIR "${PACKAGE_OUTPUT_DIR}")
    # 目录名与 Windows 侧同规则：版本号 + git 短 hash + 架构
    set(APPIMAGE_SYMBOLS_DIR
        "${PACKAGE_OUTPUT_DIR}/symbols/${APP_VERSION}-${APP_GIT_HASH}-${APP_ARCH_SUFFIX}")

    # 工具或图标缺失只告警，不阻断 configure——不做 Linux 打包的构建不该因此失败
    foreach(_tool linuxdeploy linuxdeploy-plugin-qt appimagetool)
        if(NOT EXISTS "${APPIMAGE_TOOLS_DIR}/${_tool}-${APPIMAGE_ARCH}.AppImage")
            message(WARNING
                "未找到 ${_tool}-${APPIMAGE_ARCH}.AppImage，AppImage 打包不可用。"
                "见 3rdparty/appimage-tools/README.md")
        endif()
    endforeach()
    unset(_tool)
    if(NOT EXISTS "${APPIMAGE_ICON_FILE}")
        message(WARNING
            "${APPIMAGE_ICON_FILE} 不存在；AppImage 打包需要 256x256 PNG 图标")
    endif()

    # .desktop 的字段全部由顶层定制区注入，与 .iss 的做法一致。APP_URL 为空
    # 时整行留空而非写出空的 URL=，否则桌面环境会把它当成无效链接
    if(APP_URL)
        set(APPIMAGE_URL_LINE "URL=${APP_URL}")
    else()
        set(APPIMAGE_URL_LINE "")
    endif()
    file(MAKE_DIRECTORY "${APPIMAGE_WORK_DIR}")
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/appimage.desktop.in"
        "${APPIMAGE_DESKTOP_FILE}"
        @ONLY
    )

    # 打包脚本，对应 Windows 侧的 scripts/make_installer.bat
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/make_appimage.sh.in"
        "${CMAKE_SOURCE_DIR}/scripts/make_appimage.sh"
        @ONLY
    )
    file(CHMOD "${CMAKE_SOURCE_DIR}/scripts/make_appimage.sh"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE)

    # 安装规则：打包脚本用 cmake --install 铺 AppDir，也可用于常规 Linux 安装
    install(TARGETS ${TARGET_NAME} RUNTIME DESTINATION bin)
    install(FILES "${APPIMAGE_DESKTOP_FILE}"
        DESTINATION share/applications
        RENAME "${APP_APP_NAME}.desktop")
    install(FILES "${APPIMAGE_ICON_FILE}"
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME "${APP_APP_NAME}.png")

    # make_appimage target — 对应 Windows 侧的 make_package。构建配置校验放在
    # 脚本里（而非此处），这样直接调用脚本也拦得住 Debug 打包
    add_custom_target(make_appimage
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/make_appimage.sh"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Building AppImage (${APPIMAGE_ARCH})..."
    )
    add_dependencies(make_appimage ${TARGET_NAME})
endif() # if(WIN32) / else()
