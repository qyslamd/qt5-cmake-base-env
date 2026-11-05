# OSG Demo

Qt Widgets + OpenSceneGraph 3.6.5 GLCORE 查看器示例，移植自
`qt-osg-base-env/blender-test`。

- 只保留加载/查看所需的 C++、QSS、场景 JSON、OBJ/OSG 模型和 shader。
- 不包含 Blender bpy 建模脚本；模型直接随示例作为测试资产。
- `assets/models/cow.osg` 与 `output/parts/*.obj` 用于验证模型加载、贴图、
  场景装配和 GLCORE 材质路径。

## 构建

先开启 OSG：

```bat
cmake -S . -B build\ninja-relwithdebinfo-x64 -DBUILD_OSG=ON
cmake --build build\ninja-relwithdebinfo-x64 --target osg_demo --parallel 2
```

或使用仓库脚本（OSG 缓存已在构建目录里开启时）：

```bat
scripts\build_ninja.bat -t relwithdebinfo -a x64 -j 2 -m osg_demo
```

## 运行

```bat
build\ninja-relwithdebinfo-x64\bin\osg_demo\run_osg_demo.bat
```

运行脚本会设置 `PATH`、`QT_PLUGIN_PATH` 和 `OSG_LIBRARY_PATH`，OSG DLL 与
插件仍留在构建树中，不复制到示例目录。

默认加载 `assets/scenes/desktop_pc.json`；也可以传入模型或场景：

```bat
run_osg_demo.bat -m assets\models\cow.osg
run_osg_demo.bat -m assets\scenes\desktop_pc.json
```
