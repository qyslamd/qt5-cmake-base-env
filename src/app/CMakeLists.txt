set(TARGET_NAME "demo_app")

#=======================================
# 版本管理
include(${CMAKE_SOURCE_DIR}/cmake/version_manage.cmake)
manage_version(
    VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/version.txt"
    OUTPUT_PREFIX "APP"
)

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
    splash_screen.cpp
    splash_screen.h
    log_window.cpp
    log_window.h
    switch_button.cpp
    switch_button.h
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
    demo_core
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

if(WIN32)
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/run_app.bat.in"
        "${CMAKE_BINARY_DIR}/bin/run_${TARGET_NAME}.bat"
        @ONLY
    )
endif()

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
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ISCC_APP_ID "{{028a3caf-84f9-43a5-818f-af4237aac3d6}}")
        set(ISCC_APP_PF "{autopf64}")
        set(ISCC_VC_REDIST "vc_redist.x64.exe")
        set(ISCC_USE_64BIT "YES")
    else()
        set(ISCC_APP_ID "{{c11fe96c-bb38-42e2-87ab-bd9d72fb4644}}")
        set(ISCC_APP_PF "{autopf}")
        set(ISCC_VC_REDIST "vc_redist.x86.exe")
        set(ISCC_USE_64BIT "NO")
    endif()
    set(APP_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.ico")

    #=======================================
    # 模板防呆：AppId 仍是模板默认值时提醒更换
    # Inno Setup 以 AppId 识别产品，沿用模板默认 GUID 会让基于本模板的
    # 不同项目被系统视为同一程序，安装新项目时直接覆盖卸载旧项目
    if(ISCC_APP_ID MATCHES "(028a3caf-84f9-43a5-818f-af4237aac3d6|c11fe96c-bb38-42e2-87ab-bd9d72fb4644)")
        message(WARNING
            "ISCC_APP_ID 仍是模板默认 GUID！基于本模板新建项目时，"
            "请在 src/app/CMakeLists.txt 中为 x64/x86 各换一个新 GUID"
            "（pwsh 执行 [guid]::NewGuid() 可生成），否则安装包会互相覆盖。")
    endif()

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

    # make_package target — 调用生成的 .bat 完成 ISCC 打包。
    # 该 .bat 只负责 ISCC，windeployqt 必须先行，否则 .iss 引用的
    # Qt5*.dll 等运行时文件不存在会导致打包失败，故通过目标依赖串起来
    # （而非依赖调用方先手动跑 make_deploy）。
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
    endif()
endif() # if(WIN32)
