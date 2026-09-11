# 可选 demo：目录存在才 add_subdirectory。
# 新增 demo 时只需在下方列表加一行目录名，无需再写 if(EXISTS) 块。
macro(add_optional_demo)
  foreach(demo ${ARGN})
    if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/${demo})
      add_subdirectory(${demo})
    endif()
  endforeach()
endmacro()

# 可选 demo（按需保留，未提交的本地 demo 也在此列出）
add_optional_demo(
    draw_sector_menu
    draw_ellipse_seal
    radial_text_check
    draw_bagua
)
