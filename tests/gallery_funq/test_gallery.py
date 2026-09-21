"""gallery 的 funq 用例：验证「注入 → 操作控件 → 断言属性」这条链路。

控件路径怎么来的：gallery 的控件没有 objectName，funq 的路径由「类名 + 同名
兄弟的序号后缀」拼成，且布局与分裂器会改变父子关系（源码里写
new QTabWidget(this)，实际被 addWidget 到了 QSplitter 下），所以路径必须从
运行中的进程里查，不能读源码推。查法见 probe_widgets.py。

路径段 qt_tabwidget_stackedwidget 是 Qt 给 QTabWidget 内部堆叠窗口起的
objectName，不是我们的命名。
"""

from funq.testcase import FunqTestCase

WINDOW = "GalleryWindow"
TABS = "GalleryWindow::QSplitter::QTabWidget"
PAGES = TABS + "::qt_tabwidget_stackedwidget"
BUTTONS_PAGE = PAGES + "::ButtonsDemoPage"
SECTOR_PAGE = PAGES + "::SectorMenuDemoPage"
SWITCH_BUTTON = BUTTONS_PAGE + "::QGroupBox::SwitchButton"
APPEARANCE_COMBO = "GalleryWindow::QComboBox"


class GalleryTest(FunqTestCase):
    __app_config_name__ = "gallery"

    def test_window_is_visible(self):
        """注入成功、主窗口已就绪。"""
        self.assertTrue(self.funq.widget(path=WINDOW).properties()["visible"])

    def test_tabs_have_five_pages(self):
        """读一个数值属性。"""
        self.assertEqual(self.funq.widget(path=TABS).properties()["count"], 5)

    def test_switching_tab_shows_that_page(self):
        """写属性并观察它带来的界面变化。"""
        tabs = self.funq.widget(path=TABS)
        # 非当前页不可见，取它时不能等「可见且可用」
        sector = self.funq.widget(path=SECTOR_PAGE, wait_active=False)
        try:
            tabs.set_property("currentIndex", 1)
            sector.wait_for_properties({"visible": True})
            self.assertTrue(sector.properties()["visible"])
        finally:
            tabs.set_property("currentIndex", 0)

    def test_appearance_combo_accepts_new_index(self):
        """下拉框换项：tab 栏外观切换就是靠它。"""
        combo = self.funq.widget(path=APPEARANCE_COMBO)
        try:
            combo.set_property("currentIndex", 2)
            combo.wait_for_properties({"currentIndex": 2})
            self.assertEqual(combo.properties()["currentIndex"], 2)
        finally:
            combo.set_property("currentIndex", 0)

    def test_clicking_switch_button_toggles_it(self):
        """真实点击：合成鼠标事件送到控件上。"""
        button = self.funq.widget(path=SWITCH_BUTTON)
        before = button.properties()["checked"]
        button.click()
        button.wait_for_properties({"checked": not before})
        self.assertEqual(button.properties()["checked"], not before)
