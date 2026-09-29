"""funq 靶子（examples/funq_target）的用例：验证「注入 → 操作控件 → 断言」这条链路。

控件路径怎么来的：靶子里每个被测控件都显式设了 objectName，而 funq 的
objectpath 优先用 objectName，所以路径就是 `窗口objectName::控件objectName`
——稳定、可读、改界面不必重探（旧的 gallery 用例做不到这点：它的控件没有
objectName，路径由「类名 + 同名兄弟序号」拼成，只能从运行中的进程里查）。

页签内的控件多一层 `qt_tabwidget_stackedwidget`：那是 Qt 给 QTabWidget 内部
堆叠窗口起的 objectName，不是本仓的命名，省不掉。
"""

from funq.testcase import FunqTestCase

WINDOW = "FunqTarget"
TABS = "FunqTarget::tabs"
PAGES = TABS + "::qt_tabwidget_stackedwidget"
INPUT_PAGE = PAGES + "::input_page"
DISPLAY_PAGE = PAGES + "::display_page"
ACTION_PAGE = PAGES + "::action_page"

INPUT_NAME = INPUT_PAGE + "::name_edit"
INPUT_COMBO = INPUT_PAGE + "::level_combo"
INPUT_CHECK = INPUT_PAGE + "::agree_check"
DISPLAY_STATUS = DISPLAY_PAGE + "::status_label"
ACTION_SUBMIT = ACTION_PAGE + "::submit_button"
ACTION_RESET = ACTION_PAGE + "::reset_button"
ACTION_LABEL = ACTION_PAGE + "::click_label"


class FunqTargetTest(FunqTestCase):
    __app_config_name__ = "funq_target"

    def test_window_is_visible(self):
        """注入成功、主窗口已就绪。"""
        self.assertTrue(self.funq.widget(path=WINDOW).properties()["visible"])

    def test_tabs_have_three_pages(self):
        """读一个数值属性。"""
        self.assertEqual(self.funq.widget(path=TABS).properties()["count"], 3)

    def test_switching_tab_shows_that_page(self):
        """写属性并观察它带来的界面变化。"""
        tabs = self.funq.widget(path=TABS)
        # 非当前页不可见，取它时不能等「可见且可用」
        action_page = self.funq.widget(path=ACTION_SUBMIT, wait_active=False)
        try:
            tabs.set_property("currentIndex", 2)
            action_page.wait_for_properties({"visible": True})
            self.assertTrue(action_page.properties()["visible"])
        finally:
            tabs.set_property("currentIndex", 0)

    def test_typing_updates_status_label(self):
        """跨控件联动：改输入框的文本，另一个控件跟着变。

        这条是旧 gallery 用例做不到的——那边只能把属性原样读回来，没有真实
        的业务联动可观察。
        """
        name = self.funq.widget(path=INPUT_NAME)
        status = self.funq.widget(path=DISPLAY_STATUS, wait_active=False)
        tabs = self.funq.widget(path=TABS)
        try:
            tabs.set_property("currentIndex", 1)
            status.wait_for_properties({"visible": True})
            name.set_property("text", "alpha")
            status.wait_for_properties({"text": "当前名称：alpha"})
            self.assertEqual(status.properties()["text"], "当前名称：alpha")
        finally:
            name.set_property("text", "")
            tabs.set_property("currentIndex", 0)

    def test_combo_accepts_new_index(self):
        """下拉框换项。"""
        combo = self.funq.widget(path=INPUT_COMBO)
        try:
            combo.set_property("currentIndex", 2)
            combo.wait_for_properties({"currentIndex": 2})
            self.assertEqual(combo.properties()["currentIndex"], 2)
        finally:
            combo.set_property("currentIndex", 0)

    def test_clicking_submit_increments_counter(self):
        """真实点击：合成鼠标事件送到控件上，断言业务副作用。

        断言的是自增计数（Q_PROPERTY 暴露）而不是按钮自身属性——计数变了才
        说明事件真的送到了控件的槽里。
        """
        # 动作页在 tab 2，此刻不可见，取控件时不能等「可见且可用」
        submit = self.funq.widget(path=ACTION_SUBMIT, wait_active=False)
        tabs = self.funq.widget(path=TABS)
        label = self.funq.widget(path=ACTION_LABEL, wait_active=False)
        try:
            tabs.set_property("currentIndex", 2)
            submit.wait_for_properties({"visible": True})
            submit.click()
            submit.click()
            label.wait_for_properties({"text": "提交 2 次"})
            self.assertEqual(label.properties()["text"], "提交 2 次")
            self.assertEqual(self.funq.widget(path=WINDOW)
                             .properties()["submit_count"], 2)
        finally:
            self.funq.widget(path=ACTION_RESET).click()
            tabs.set_property("currentIndex", 0)
