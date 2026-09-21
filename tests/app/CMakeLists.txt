# demo_app 的 UI 回放测试：起被测程序，回放一份事件脚本，按退出码判定。
#
# 与 add_unit_test 那套 gtest 用例不同——UI 测试的对象是完整进程（要真实
# 的窗口与事件循环），所以这里直接 add_test 拉起 demo_app，不做 gtest 链接。
#
# 脚本由人工录制（Debug 构建启动 demo_app，菜单「测试 → 录制事件」）后放入
# fixtures/。当前这份只含一条空事件序列：它验证的是「应用能起来、QtTesting
# 管线能装载脚本、播完能按结果退出」这条链路本身，不覆盖任何界面逻辑。
if(BUILD_QT_TESTING)
    add_test(NAME test_app_uitest_playback
        COMMAND $<TARGET_FILE:${APP_TARGET_PREFIX}_app>
            --play "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/smoke_events.xml"
    )
    set_tests_properties(test_app_uitest_playback PROPERTIES
        TIMEOUT ${TEST_DEFAULT_TIMEOUT}
        LABELS "APP;QTTESTING"
        ENVIRONMENT "${TEST_ENV_QT}"
    )
endif()
