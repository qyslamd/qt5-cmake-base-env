# collect_funq_qt_deps.cmake
# 给随包分发的 funq 工具补齐"注入库需要、主程序不需要"的 Qt 运行时 DLL，
# 拷进 funq\qt\（启动脚本把该目录挂进 PATH，被测程序继承之，注入库的导入
# 因此能解析）。
#
# 为什么不能只靠 windeployqt：它按**主程序**的依赖决定部署哪些 Qt DLL，而
# libFunQ.dll 是在被测程序里被 LoadLibrary 载入的——它有自己的依赖。本仓实测
# 缺的是 Qt5Test.dll（funq 的库链了 Qt5::Test，而主程序在 BUILD_QT_TESTING=OFF
# 时不链），症状极具误导性：LoadLibraryW 返回 0、last error 为 0，runner 报的
# 是"Maybe x86/x64 mismatch between python.exe and Qt DLLs?"——看上去像位数问题，
# 实际是缺一个 DLL。
#
# 判断必须放在**构建期**：bin\ 里的那些 DLL 要 windeployqt 跑完才有，配置期
# 看到的永远不全。故由 src/app/CMakeLists.txt 的 make_funq_kit 目标以 -P 调用。
#
# 入参（都是绝对路径或列表）：
#   QT_BIN_DIR       Qt 的 bin 目录
#   BIN_DIR          主程序所在目录（windeployqt 的产物）
#   DEST_DIR         工具目录下的 qt\ 子目录
#   FUNQ_QT_REQUIRED 注入库需要的 Qt 模块名列表（无扩展名）

if(NOT EXISTS "${BIN_DIR}")
    message(FATAL_ERROR "bin dir not found: ${BIN_DIR}")
endif()

# 每次都重建：上一次跑可能往这儿放过某些 DLL，而这一次主程序自己就带了——
# 留着会是重复的一份（同版本，无害，但没必要），清掉更可预期
file(REMOVE_RECURSE "${DEST_DIR}")
file(MAKE_DIRECTORY "${DEST_DIR}")

file(GLOB present_dlls "${BIN_DIR}/*.dll")
set(present_names "")
foreach(dll_file ${present_dlls})
    get_filename_component(dll_name "${dll_file}" NAME)
    list(APPEND present_names "${dll_name}")
endforeach()

set(copied "")
foreach(module ${FUNQ_QT_REQUIRED})
    set(dll_name "${module}.dll")
    if(dll_name IN_LIST present_names)
        continue()
    endif()
    if(EXISTS "${QT_BIN_DIR}/${dll_name}")
        file(COPY "${QT_BIN_DIR}/${dll_name}" DESTINATION "${DEST_DIR}")
        list(APPEND copied "${dll_name}")
    else()
        message(WARNING
            "funq kit: 注入库需要 ${dll_name}，但 ${QT_BIN_DIR} 里没有——"
            "随包工具注入时可能报 LoadLibrary 失败")
    endif()
endforeach()

if(copied)
    message(STATUS "funq kit: 已补入注入库独有的 Qt DLL: ${copied}")
else()
    message(STATUS "funq kit: 注入库的 Qt 依赖主程序都已带，无需补")
endif()
