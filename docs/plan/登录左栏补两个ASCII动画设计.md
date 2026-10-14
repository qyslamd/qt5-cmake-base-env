# 登录左栏补两个 ASCII 动画设计

状态：**一轮审查 6🟡+6🔵 已处置**（2026-10-14），待实施。

## 一、问题与目标

登录窗左栏（`CharacterPanel`）现在只有两支动画策略：默认的四角色动效与它的
静止版。策略化重构的意义就是"换一个 `CharacterAnimator` 就换一种画面"，现在
补两支新画面——把 `qt-libui` 仓库 `examples/just_for_fun` 里的两个字符场景
搬过来：

- **电视雪花 TvStatic**（58×26）：满屏雪花 + 交流声亮带滚动 + 旋钮咔哒后
  测试卡滚进画面再丢失
- **火箭发射 Rocket**（48×28）：夜间发射，boil-off 排气 → 点火 → 升空出画
  → 着陆点火降回

两者都是 `(QChar, colorIdx)` 字符缓冲 + 等宽字体逐格画的路子，与既有策略的
"QPainter 画形状"不同，需要连绘制端一起搬。

顺带把登录族收口：`dialogs/` 平铺 23 个文件、登录族占 9 个（ascii 后 14 个），
趁这次挪进 `dialogs/login/`（纯移动单独一笔提交，见 §七）。

## 二、素材盘点（实据）

同族共四个字符场景（Rocket / Hourglass / Train / TvStatic），另五个半调点阵
场景（走 OpenGL/点阵，不在本次范围）。

- **接口一致**：`static constexpr kWidth/kHeight/kPaletteN` + `PaletteHex()` +
  `Update(double t)` + `charBuf()`/`colorBuf()`（四个头逐条核过）
- **自包含**：只依赖 `QChar/QString/algorithm/cmath`；哈希/噪声各自在 cpp 内
  私有实现，不引用 `halftone_common.h`（那是半调场景的）
- **绘制端** `text_view.h` 的 `TextView<Scene>`：等宽字体、逻辑格宽 1 高 2、
  按 min(scale) 缩放居中、空格跳过、字号 `max(4, int(cellH*scale*0.98))`、
  20fps（`1000/20`）、构造时 `scene_.Update(0.0)`
- **底色由调用方给**：Rocket `qRgb(0,0,0)`、TvStatic `qRgb(0x0b,0x08,0x14)`
  （`widget.cpp:68-74` 的注册处）
- **时间基准**：任意 `t` 都成立（内部 `fmod` 周期 + 负值保护），传"从 0 起算
  的秒"与原版一致
- **实测（审查期）**：两个 cpp 原样 + `TextView<Scene>` 实例化，用本仓工具链
  （VS2019/Qt5.15.2/`/std:c++17 /W4 /utf-8`）编译 **0 error / 0 warning**；
  `rocket.cpp` 的 UTF-8 字面量与顶层已有的 `/utf-8` 开关配套，无需额外处理
- **出处与许可**：原作 `github.com/bas3line/ascii`（TypeScript, MIT），
  qt-libui 里已是 C++ 直译；MIT 兼容，移植保留出处注释

## 三、方案

### 3.1 落点与目录

```
src/app/dialogs/login/            登录族收口（§七 先做纯移动）
  login_window.{h,cpp}
  login_animator_factory.{h,cpp}  按配置造策略（四个值都认识）
  character_animator.h
  character_panel.{h,cpp}
  default_character_animator.{h,cpp}
  static_character_animator.{h,cpp}
  ascii/
    ascii_scene_animator.h        模板壳：渲染 + 帧驱动
    tv_static.{h,cpp}             移植
    rocket.{h,cpp}                移植
```

**场景按原样移植**，只做五件事：头保护宏（`FILE_APP_DIALOGS_ASCII_*_H_`）、
保留出处注释、**补花括号**（素材里大量单行 `if/for` 无 `{}`，而本仓
`clang-format` 实测不插括号，只有 `AllowShortIfStatementsOnASingleLine:
Never`）、**`rocket.cpp:103` 的局部量 `BODY` 改名**（全大写但不是宏，与
kPascalCase 规则冲突）、过 `clang-format`（`rocket.cpp:24-48` 的 ASCII 画
已有 `// clang-format off/on`，保留）。不加命名空间；素材里的制表符/块元素
（`0x2550`/`0x2588` 等）不是 emoji，不触禁用条。

### 3.2 渲染器与策略壳

`AsciiSceneAnimator<Scene> : CharacterAnimator`——**模板而非两个具体类**，
绘制逻辑只写一份。模板类不声明 `Q_OBJECT`（AUTOMOC 宏名表只认
`Q_OBJECT`/`Q_GADGET`/`Q_NAMESPACE`，模板头里不出现就不跑 moc；注释里也别
写这个词），实测在本仓编得过，且 `connect` 走基类指针
（`character_panel.cpp:63-64`）不受影响。**代价要记住**：该类型只当
`CharacterAnimator*` 用——`qobject_cast`/`findChild` 对它不可用（Qt 5.15 对
无 Q_OBJECT 类型直接 static_assert）；日后真需要，退化成两个带 `Q_OBJECT`
的具体类即可。

```cpp
explicit AsciiSceneAnimator(QRgb bg, QObject* parent = nullptr);
```

- **底色是构造参数**（场景类不给底色，与原 `TextView(QRgb, QWidget*)` 同形），
  别名只收口类型：`using TvStaticAnimator = AsciiSceneAnimator<TvStatic>;`
  工厂按 §二 的注册值给色
- **构造期义务**：`scene_.Update(0.0)` + `elapsed_.start()`。前者防首帧空白
  （`setAnimator()` 末尾立刻 `update()`，没 Update 过就是一屏空格）；后者防
  UB（未 start 的 `QElapsedTimer::elapsed()` 是未定义值，喂给
  `floor(t*kFps)` 的越界 double→int 是 UB）
- `isAnimated()` → true；`animationIntervalMs()` → **50**（20fps，同原版）
- `tick()`：`scene_.Update(elapsed_.elapsed() / 1000.0)`
- `paint()`：铺底色 → `min(w/kWidth, h/(kHeight*2))` 缩放居中 → 等宽字体
  （`QFontDatabase::systemFont(FixedFont)`）逐格画，空格跳过
- `reset()`：**不覆写**——面板只在"替换旧策略"与析构时调用它
  （`character_panel.cpp:46,56`），那时本对象随即销毁，没有挂起任务要作废；
  去动 `elapsed_`（如 `invalidate()`）反而会留下未启动的计时器（实现时定的）
- **尺寸预期**（按左栏 550×700 与最小窗 450×580 算）：TvStatic 字号 18px /
  15px，画面竖向只占 56%~70%，上下留白约百来像素；Rocket 22px / 18px，几乎
  顶满。都是**按最小边缩放居中的预期结果**，不做裁切/拉伸

### 3.3 谁装、怎么换

默认策略仍是四角色动效，不动。新增配置键 **`login/animation`**
（`config_defs.h`：`kConfigLoginAnimation` + 四个取值常量，风格照
`crash/backend`；**缺省值注册进 `initDefaultValues()`**，这样可被
`resetKeyToDefault()` 复位、也进 `exportToJson`）。

`LoginWindow::initUi()` 末尾按它装策略。**取值必须传 QString**：工厂里写
`ConfigManager::instance().get<QString>(QString::fromLatin1(kConfigLoginAnimation),
QString::fromLatin1(kConfigLoginAnimationDefault))`——传字面量会推导成
`char[8]` 命中 `QVariant::fromValue` 的 static_assert（审查期实测编译不过）。
未知值回落 default 并记一条 `LOG_WARN`。该键**只在构造时读一次**，不接
`configChanged`（与"不做设置界面选择项"一致）。

工厂住 `login/login_animator_factory.{h,cpp}`（认识四个值：default / static /
tv / rocket），落在 login 目录而不是 ascii 目录——它认识的两支策略不在 ascii
里。`characterPanel()->setAnimator()` 这条程序化出口保持不变。

### 3.4 与登录窗其余部分的关系

换到 ASCII 策略时左栏只有场景本身（没有 logo 与页脚链接——那是默认策略画的
内容），右栏表单与交互完全不变；`typingChanged` 等事件照发，策略不理会而已
（`AsciiSceneAnimator` 不覆写 `setInput`，用基类存的 `input_`）。

## 四、非目标

- 不移植五个半调点阵场景
- 不做设置界面里的动画选择项（只走配置键，`--set` 可覆盖）
- 不改默认（仍是四角色动效）

## 五、验收

1. `scripts\build_ninja.bat -t relwithdebinfo -a x86` 编过；ctest 与改动前一致
   （无既有测试依赖 animator，审查已 grep 证实）
2. 看两支新动画：**前置是登录窗得出现**——现成的 `funq_app.conf` 带
   `--set login/enabled=false`，照抄看不到登录窗。做法：`temp/` 下写一份
   指向 demo_app 的 conf（args 去掉那条、加 `--set login/animation=tv`），
   用 `funq_attach` 起，再按 oid 抓 `loginDialog::loginLeftPanel`
3. 每支**隔 1s 连抓 3 张**：TvStatic 的 10s 循环与 Rocket 的 18s 循环都有
   静止段，"两张不一致"可能撞相位；三张能同时证明"画面在动"
4. 未知取值（如 `=foo`）回落默认策略且日志有 WARN
5. qss 路与 fluent 路都能编（策略与路线无关，但两条都要过）
6. 顺带看一眼进程 CPU：TvStatic 每帧约 1300 次 `drawText` × 20fps。原版就
   这么画、能跑；若占用偏高，第一手段是 `QStaticText` 或字形缓存

## 六、决策记录（2026-10-14）

1. **搬 TvStatic + Rocket 两支**（用户点名电视，第二支从 Hourglass / Train /
   Rocket 里选了 Rocket）
2. **模板壳 `AsciiSceneAnimator<Scene>`**：不为两支各写一份绘制；代价是无
   `Q_OBJECT`（§3.2 已写明限制面）
3. **落 `src/app/dialogs/login/ascii/`**，场景按原样移植（五件事见 §3.1）
4. **配置键 `login/animation`** 作切换口，不碰设置界面；缺省值注册进
   `initDefaultValues()`
5. **登录族先收口进 `dialogs/login/`**（纯移动一笔，见 §七）
6. **底色走构造参数**，别名只收口类型（审查 🟡1）
7. **`reset()` 只停时间基准**，不复位相位（审查 🟡3：面板只对旧策略调它）
