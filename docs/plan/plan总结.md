# plan 文档归档总结

`docs/plan/` 只留还在进行的方案；收尾的方案提炼到这里后删原文，原文用
`git log --diff-filter=D -- docs/plan/<文件名>` 查。

## 单实例 + 崩溃捕获接入，与 Linux 构建支持（2026-09-11，原同名文档）

**仍有效**

- D2 单实例键取 `applicationName()`，对占位值 `"FILE"` 防呆告警
- D4 Linux 边界：configure + build + test（+ AppImage），不移植
  windeployqt / Inno Setup
- D6 致命信号归 CrashHandler，GEH 只管 Qt 消息重定向 + SIGINT/SIGTERM，
  **不是**双层防护 → `main.cpp:282`

**坑**

- include 大小写：Windows 不敏感，Linux 直接编译失败
- `#ifdef _DEBUG` 是 MSVC 专属宏，GCC 下 Debug 不定义；判断调试构建用 `NDEBUG`
- 非 Windows 没有 HWND 激活那条路，靠 `showApplication` 信号手动显示窗口

**已作废**：§3.5 给 Linux 的命令是 `cmake --preset ...`（Linux 侧不走 preset，
`binaryDir` 与 Windows 侧同名会互相覆盖缓存），现用 `scripts/build_ninja.sh`。
§6 遗留项了结三条，只剩 `single_instance.cpp` 的 `_WIN32` 守卫风格（MSVC-only，不改）。

## guidance 模块的四处缺陷（2026-09-22，原《引入的guidance的bug记录》）

- B1 / B2 模式状态存了两份（控制器 + 演示层），跨主题只更新一份；且
  `auto_mode_` 全仓没有置回 true 的路径 → 收口成一个 `syncPresentationMode()`
- B3 `time_percent_` 的语义是"已过去的比例"，初值给反成了 1.0
- B4 **被显式 `hide()` 过的子控件，不会随父控件 `show()` 回来**
  （`showChildren()` 跳过 `isHidden()` 的子控件）→ `guidance_presentation.cpp:387`

B1 / B2 只在多主题、且其中一条是实操式时显形；`guidance_demo` 三条主题跑一遍
「全部演示」即覆盖。未做完的两条 → `挂起事项.md` 第 6、7 条。

## libui 抽离为独立 CMake 库（2026-09-14，原同名文档）

抽出 `src/libui`（40 文件 / 3196 行）连同 gallery 成独立仓 `qt-libui`。
**接入方式后反转**：§9 判定"已评估未采用"的 submodule 最终被采纳（源码内嵌，
只取 `qt-libui::libui` 目标、不登记 install 规则）。

**坑**

- Debug（`/MDd`）链 RelWithDebInfo 静态库（`/MD`）是 `LNK1319 FAILIFMISMATCH`：
  **fatal，不是设计时判断的"可接受警告"**
- 装出去的头文件必须带 UTF-8 BOM，否则消费方不开 `/utf-8` 会按 936 代码页
  解析、报 `C2059`（本仓全局 `/utf-8`，不受影响）
- `GNUInstallDirs` 要早于 `add_subdirectory(src)`；`INSTALL_INTERFACE` 与
  `install(... INCLUDES DESTINATION)` 写两遍会让包含目录重复；头文件装到
  `include/qt-libui/` 时搜索根取上一级
- 库内源文件只能"同目录裸名"包含，写成从 src 根出发的路径：库编得过、
  消费方必炸

**仍有效**：QSS 与 libui 的 objectName 构成跨仓隐式契约（`#splashContent` 等
5 个），改一侧另一侧的样式会静默失效；libui 冻结（见 `挂起事项.md`）。

## 不归档

- `guidance接入funq.md`——四条约束（可见 QWidget、早于登录、单 QVariant
  签名、只认最后一段 objectName）目前只记在它里面
- `挂起事项.md`——活清单
