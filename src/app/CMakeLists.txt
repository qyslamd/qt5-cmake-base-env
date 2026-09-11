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
    ${APP_TARGET_PREFIX}_core
    ${APP_TARGET_PREFIX}_libui
    qcustomplot
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

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
# 安装打包工具链（windeployqt / ISCC 均为 Windows 专属工具）
#=======================================
if(WIN32)
    # 查找 windeployqt — 从 Qt5::qmake 目标推导 bin 目录
    get_target_property(_qt_qmake Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${_qt_qmake}" DIRECTORY)

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
endif() # if(WIN32)
