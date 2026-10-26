add_subdirectory(style)
add_subdirectory(tour)
# logging 要排在 core 之前：core 的日志门面链它，目标得先存在
add_subdirectory(logging)
add_subdirectory(core)
add_subdirectory(custom_controls)
add_subdirectory(app)
