# QCustomPlot 静态库
add_library(qcustomplot STATIC qcustomplot.cpp)

# QCustomPlot 的 MOC 元对象通过 #include "moc_qcustomplot.cpp" 在 qcustomplot.cpp 末尾手动包含
# 需启用 AUTOMOC 以自动生成 moc 文件
set_target_properties(qcustomplot PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

target_include_directories(qcustomplot PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(qcustomplot PUBLIC Qt5::Core Qt5::Widgets Qt5::PrintSupport)
if(MSVC)
    target_compile_options(qcustomplot PRIVATE /utf-8)
endif()
