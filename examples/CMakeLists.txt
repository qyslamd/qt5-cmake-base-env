add_subdirectory(fft_demo)
add_subdirectory(funq-scripts)
add_subdirectory(funq_target)
add_subdirectory(tour_demo)
add_subdirectory(style_sheet_demo)

if(BUILD_OSG AND TARGET OSG::osgViewer)
    add_subdirectory(osg_demo)
endif()
