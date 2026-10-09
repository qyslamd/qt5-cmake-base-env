# Handler executable
add_library(crashpad_handler_obj OBJECT)

set_target_properties(crashpad_handler_obj PROPERTIES
    CXX_STANDARD 14
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET "hidden"
    C_VISIBILITY_PRESET "hidden"
    VISIBILITY_INLINES_HIDDEN ON
)

target_link_libraries(crashpad_handler_obj PRIVATE
    minichromium
    crashpad_common
    crashpad_compat
    crashpad_client
    crashpad_minidump
    crashpad_snapshot
    crashpad_tools
    ZLIB::ZLIB
    AppleFrameworks
)

target_sources(crashpad_handler_obj PRIVATE
    ${crashpad_git_SOURCE_DIR}/handler/crash_report_upload_thread.cc
    ${crashpad_git_SOURCE_DIR}/handler/handler_main.cc
    ${crashpad_git_SOURCE_DIR}/handler/minidump_to_upload_parameters.cc
    ${crashpad_git_SOURCE_DIR}/handler/prune_crash_reports_thread.cc
    ${crashpad_git_SOURCE_DIR}/handler/user_stream_data_source.cc
)

if(APPLE)
    target_sources(crashpad_handler_obj PRIVATE
        ${crashpad_git_SOURCE_DIR}/handler/mac/crash_report_exception_handler.cc
        ${crashpad_git_SOURCE_DIR}/handler/mac/exception_handler_server.cc
        ${crashpad_git_SOURCE_DIR}/handler/mac/file_limit_annotation.cc
    )
	# Hack to fix upstream backtrace fork not updating this var in line with minichromium.
	target_compile_definitions(crashpad_handler_obj PRIVATE OS_MACOSX=1)
endif()

if(UNIX AND NOT APPLE)
    target_sources(crashpad_handler_obj PRIVATE
        ${crashpad_git_SOURCE_DIR}/handler/linux/capture_snapshot.cc
        ${crashpad_git_SOURCE_DIR}/handler/linux/crash_report_exception_handler.cc
        ${crashpad_git_SOURCE_DIR}/handler/linux/exception_handler_server.cc
    )

    if(NOT ANDROID)
        target_sources(crashpad_handler_obj PRIVATE
            ${crashpad_git_SOURCE_DIR}/handler/linux/cros_crash_report_exception_handler.cc
        )
    endif()
endif()

if(WIN32)
    target_sources(crashpad_handler_obj PRIVATE
        ${crashpad_git_SOURCE_DIR}/handler/win/crash_report_exception_handler.cc
    )
endif()

add_executable(crashpad_handler ${crashpad_git_SOURCE_DIR}/handler/main.cc)

set_target_properties(crashpad_handler PROPERTIES
    CXX_STANDARD 14
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET "hidden"
    C_VISIBILITY_PRESET "hidden"
    VISIBILITY_INLINES_HIDDEN ON
    # 必须是 GUI 子系统。上游把 handler 编成控制台程序（它的 LOG 写 stderr，
    # 手工跑看得见），而主程序是 GUI：**GUI 进程起一个控制台子进程时，Windows
    # 会给它新开一个控制台窗口**（crashpad 的 CreateProcess 没带
    # CREATE_NO_WINDOW，见 client/crashpad_client_win.cc）。于是双击主程序，
    # 旁边一直挂着一个黑框——实测 crashpad_handler.exe 的 PE 子系统是
    # WINDOWS_CUI。改成 GUI 之后它照样收 dump，只是 stderr 没人接
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(crashpad_handler PRIVATE
    minichromium
    crashpad_common
    crashpad_compat
    crashpad_client
    crashpad_minidump
    crashpad_snapshot
    crashpad_tools
    crashpad_handler_obj
    ZLIB::ZLIB
    AppleFrameworks
)
