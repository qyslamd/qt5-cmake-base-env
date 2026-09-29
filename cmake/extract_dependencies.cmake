# extract_dependencies.cmake - 自动解压第三方依赖库
# 在顶层CMakeLists.txt中调用，用于解压3rdparty目录下的tar.gz文件

# 设置3rdparty目录路径
set(3RDPARTY_DIR "${CMAKE_SOURCE_DIR}/3rdparty")

# 定义需要解压的依赖库列表
set(DEPENDENCY_ARCHIVES
    "googletest-1.17.0.tar.gz"
    "spdlog-1.17.0.tar.gz"
    "Inno Setup 6.zip"
    "QXlsx-1.5.0.tar.gz"
    "funq-master.tar.gz"
    "QtTesting.tar.gz"
    "SARibbon-2.5.7.tar.gz"
    "qwindowkit-1.5.0.tar.gz"
    "qmsetup-4a3ff82.tar.gz"
    "fftw-3.3.11.tar.gz"
    "zlib-v1.2.11.tar.gz"
    "mini_chromium-9cdc2a7.tar.gz"
    "crashpad-7b9686b.tar.gz"
    "linux-syscall-support-e1e7b0a.tar.gz"
)

# 检测tar解压命令是否可用
function(check_tar_command)
    execute_process(COMMAND tar --version
                    OUTPUT_VARIABLE tar_version
                    ERROR_QUIET
                    RESULT_VARIABLE tar_check)
    if(tar_check EQUAL 0)
        set(TAR_FOUND TRUE PARENT_SCOPE)
        message(STATUS "Found tar command, will use tar for extraction")
    else()
        set(TAR_FOUND FALSE PARENT_SCOPE)
        message(STATUS "tar command not found, checking 7z...")
    endif()
endfunction()

# 定位 7z 可执行文件
#
# 优先使用 3rdparty 自带的 7z，使使用者无需预先安装解压软件；仓库未提供
# 或文件缺失时回落到 PATH 上的 7z。SEVEN_ZIP_EXECUTABLE 由 find_program
# 写入缓存，故无需 PARENT_SCOPE。
function(check_7z_command)
    find_program(SEVEN_ZIP_EXECUTABLE
        NAMES 7z 7za
        HINTS "${3RDPARTY_DIR}"
    )
    if(SEVEN_ZIP_EXECUTABLE)
        set(7Z_FOUND TRUE PARENT_SCOPE)
        message(STATUS "Found 7z: ${SEVEN_ZIP_EXECUTABLE}")
    else()
        set(7Z_FOUND FALSE PARENT_SCOPE)
        message(STATUS "7z command not found")
    endif()
endfunction()

# 解压函数
function(extract_if_needed archive_file)
    set(archive_path "${3RDPARTY_DIR}/${archive_file}")
    
    # 检查压缩包是否存在
    if(NOT EXISTS "${archive_path}")
        message(WARNING "Archive not found: ${archive_file}, skipping...")
        return()
    endif()
    
    # 提取目录名（去掉.tar.gz、.zip或.7z后缀）
    string(REGEX REPLACE "\\.(tar\\.gz|zip|7z)$" "" dir_name "${archive_file}")
    set(dir_path "${3RDPARTY_DIR}/${dir_name}")
    
    # 检查目录是否已存在
    if(EXISTS "${dir_path}")
        message(STATUS "Already extracted: ${archive_file}")
        return()
    endif()
    
    # 尝试解压
    message(STATUS "Extracting: ${archive_file}")

    # 判断文件类型：
    # - .tar.gz 使用 tar 命令解压
    # - .zip 使用 CMake 内置的 file(ARCHIVE_EXTRACT) 解压
    # - .7z 使用 7z 命令解压
    # python 的 embeddable zip 与别的包不同：里面**没有顶层目录**（python.exe 就在
    # 根上），而通用分支是直接解到 3rdparty/ 的——照那条走会把 python.exe、DLL、
    # 标准库全撒在 3rdparty 根下。这类包按"自己一个目录"解
    if("${archive_file}" MATCHES "^python-.*-embed-.*\\.zip$")
        message(STATUS "  Detected embedded-python zip (no top level dir), extracting into its own folder...")
        file(MAKE_DIRECTORY "${dir_path}")
        file(ARCHIVE_EXTRACT INPUT "${archive_path}" DESTINATION "${dir_path}")
        set(extract_success TRUE)
    elseif("${archive_file}" MATCHES "\\.zip$")
        message(STATUS "  Detected zip format, using CMake built-in extract...")
        file(ARCHIVE_EXTRACT INPUT "${archive_path}" DESTINATION "${3RDPARTY_DIR}")
        set(extract_success TRUE)
    elseif("${archive_file}" MATCHES "\\.7z$")
        message(STATUS "  Detected 7z format, looking for 7z command...")
        set(extract_success FALSE)
        if(7Z_FOUND)
            message(STATUS "  Extracting with 7z...")
            execute_process(COMMAND "${SEVEN_ZIP_EXECUTABLE}" x "${archive_file}" -y
                            WORKING_DIRECTORY "${3RDPARTY_DIR}"
                            RESULT_VARIABLE extract_result
                            OUTPUT_QUIET)
            if(extract_result EQUAL 0)
                set(extract_success TRUE)
                message(STATUS "  Successfully extracted using 7z")
            else()
                message(STATUS "  7z finished with warnings (exit code ${extract_result}), checking directory...")
            endif()
        else()
            message(FATAL_ERROR "  7z format requires 7z command, but 7z was not found. Please install 7-Zip.")
        endif()
    else()
        set(extract_success FALSE)

        # 优先使用tar命令
        if(TAR_FOUND)
            execute_process(COMMAND tar -xf "${archive_file}"
                            WORKING_DIRECTORY "${3RDPARTY_DIR}"
                            RESULT_VARIABLE extract_result
                            OUTPUT_QUIET)
            if(extract_result EQUAL 0)
                set(extract_success TRUE)
                message(STATUS "  Successfully extracted using tar")
            else()
                message(STATUS "  tar finished with warnings (exit code ${extract_result}), checking directory...")
            endif()
        endif()

        # 如果tar失败或不存在，尝试7z
        if(NOT extract_success AND 7Z_FOUND)
            message(STATUS "  Trying 7z...")
            execute_process(COMMAND "${SEVEN_ZIP_EXECUTABLE}" x "${archive_file}" -y
                            WORKING_DIRECTORY "${3RDPARTY_DIR}"
                            RESULT_VARIABLE extract_result
                            OUTPUT_QUIET)

            # .tar.gz 是两层压缩：首次 x 只解出内层的 .tar，必须对它再解一次
            # 才能得到目录（实测单次 x 只得到 spdlog-1.17.0.tar）。
            # 无内层归档时同名为真，直接跳过。
            string(REGEX REPLACE "\\.gz$" "" inner_archive "${archive_file}")
            if(NOT "${inner_archive}" STREQUAL "${archive_file}")
                if(EXISTS "${3RDPARTY_DIR}/${inner_archive}")
                    message(STATUS "  Nested archive detected, extracting ${inner_archive}...")
                    execute_process(
                        COMMAND "${SEVEN_ZIP_EXECUTABLE}" x "${inner_archive}" -y
                        WORKING_DIRECTORY "${3RDPARTY_DIR}"
                        RESULT_VARIABLE extract_result
                        OUTPUT_QUIET)
                    # 内层 .tar 只是解压过程的中间产物，用完即删，
                    # 否则 3rdparty 下会残留一份与压缩包等大的文件
                    file(REMOVE "${3RDPARTY_DIR}/${inner_archive}")
                endif()
            endif()

            if(extract_result EQUAL 0)
                set(extract_success TRUE)
                message(STATUS "  Successfully extracted using 7z")
            else()
                message(STATUS "  7z finished with warnings (exit code ${extract_result}), checking directory...")
            endif()
        endif()
    endif()

    # 验证解压结果：目录存在且非空即视为成功
    # Windows 内置 tar.exe 对含中文文件的压缩包报错退出，
    # 但实际源文件已正确解出，不影响编译
    if(EXISTS "${dir_path}")
        file(GLOB dir_content "${dir_path}/*")
        if(dir_content)
            message(STATUS "  Extraction complete: ${dir_name}")
        else()
            message(FATAL_ERROR "  Extraction failed: ${dir_name} is empty")
        endif()
    else()
        message(FATAL_ERROR "  Extraction failed for: ${archive_file}")
    endif()
endfunction()

# python embeddable 的 sys.path 由紧挨着 python.exe 的 python38._pth 决定，而且
# 一旦存在这个文件，解释器就进**隔离模式**：**PYTHONPATH 会被整个忽略**（实测踩过，
# 当时误以为是"没写 import site"——写 `import site` 只补 <home>/site-packages，
# 并不能让 PYTHONPATH 生效）。所以 funq 那两个包目录只能**列在这里**：
#
#   ..\funq-master\client / server    解析自本文件所在目录（3rdparty/python-*），
#                                     于是 -DFUNQ_PYTHON 指向这份解释器时，注入
#                                     脚本里那句 PYTHONPATH 有没有都无所谓
#
# 每次 configure 重写一遍（幂等且便宜）：解压产物会被重解压覆盖，而这几行必须始终在
function(configure_embedded_python_pth dir_name)
    set(dir_path "${3RDPARTY_DIR}/${dir_name}")
    if(NOT EXISTS "${dir_path}")
        return()
    endif()
    file(GLOB pth_files "${dir_path}/*._pth")
    foreach(pth_file ${pth_files})
        file(WRITE "${pth_file}"
            "python38.zip\n.\n..\\funq-master\\client\n..\\funq-master\\server\n")
        message(STATUS "  [OK]   ${dir_name} 的 ._pth 已写入（含 funq 的两个包目录）")
    endforeach()
endfunction()

# 主执行逻辑
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Checking 3rdparty dependencies...")
message(STATUS "========================================")

# 检测解压命令
check_tar_command()
check_7z_command()

if(NOT TAR_FOUND AND NOT 7Z_FOUND)
    message(FATAL_ERROR "No extraction tool found! Please install either tar (Windows 10+ built-in) or 7-Zip")
endif()

# 遍历所有依赖库进行解压
foreach(archive_file ${DEPENDENCY_ARCHIVES})
    extract_if_needed(${archive_file})
endforeach()

# python 只 Windows 需要（Linux 用系统 python3），所以不进上面那张"所有平台"的
# 清单。两份都解出来，用哪一份由消费方按目标架构决定（见 tests/funq_demo 与
# src/app/CMakeLists.txt 里打安装包的那段）
if(WIN32)
    extract_if_needed("python-3.8.8-embed-amd64.zip")
    extract_if_needed("python-3.8.8-embed-win32.zip")
    configure_embedded_python_pth("python-3.8.8-embed-amd64")
    configure_embedded_python_pth("python-3.8.8-embed-win32")
endif()

message(STATUS "========================================")
message(STATUS "Dependency extraction complete!")
message(STATUS "========================================")
message(STATUS "")


#-------------------------------------------------------------------------------
# 生成「清理解压产物」脚本（按平台二选一）
#
# 与 make_installer.bat / make_appimage.sh 同构：Windows 出 .bat，Linux 出 .sh，
# 当前平台用不到的那个不生成——configure 本来就跑在具体平台上，没必要往
# 源码树里塞一份对面平台的脚本。
#
# 目录列表从 DEPENDENCY_ARCHIVES 派生：加/删依赖时清理脚本自动跟上，不会出现
# 「清单和实际解压的集合对不上」。只删解压出来的目录，压缩包本身保留。
#-------------------------------------------------------------------------------

# 已解压目录名（去掉压缩后缀），与 extract_if_needed 里的推导规则保持一致
set(EXTRACTED_DIR_NAMES "")
foreach(archive_file ${DEPENDENCY_ARCHIVES})
    string(REGEX REPLACE "\\.(tar\\.gz|zip|7z)$" "" dir_name "${archive_file}")
    list(APPEND EXTRACTED_DIR_NAMES "${dir_name}")
endforeach()

# 两个模板占位符内容一致：每行一个带引号的目录名。
# 引号必须带——"Inno Setup 6" 含空格，不加会被 for/数组拆成两个 token
set(CLEAN_DEPS_DIRS "")
foreach(dir_name ${EXTRACTED_DIR_NAMES})
    string(APPEND CLEAN_DEPS_DIRS "    \"${dir_name}\"\n")
endforeach()

if(WIN32)
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/template/clean_deps.bat.in"
        "${CMAKE_SOURCE_DIR}/scripts/clean_deps.bat"
        @ONLY
    )
    message(STATUS "已生成清理脚本: scripts/clean_deps.bat")
else()
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/template/clean_deps.sh.in"
        "${CMAKE_SOURCE_DIR}/scripts/clean_deps.sh"
        @ONLY
    )
    file(CHMOD "${CMAKE_SOURCE_DIR}/scripts/clean_deps.sh"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE)
    message(STATUS "已生成清理脚本: scripts/clean_deps.sh")
endif()
