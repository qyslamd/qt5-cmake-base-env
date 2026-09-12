# AppImage 打包工具

Linux 侧 AppImage 打包用的三个官方工具，**随仓库提供**，打包时无需联网。
`scripts/make_appimage.sh`（由 CMake 生成）调用它们。

| 文件 | 用途 | 版本 |
| --- | --- | --- |
| `linuxdeploy-x86_64.AppImage` | 铺 AppDir、收集依赖、驱动插件 | 1-alpha，build 368（2026-09-01） |
| `linuxdeploy-plugin-qt-x86_64.AppImage` | 打包 Qt 库、平台插件与翻译 | 与 linuxdeploy 同批次，无独立版本号 |
| `appimagetool-x86_64.AppImage` | 由 AppDir 产出 AppImage | continuous，build 295（2025-12-04） |

下载地址（均为上游 `continuous` 通道）：

- <https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage>
- <https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage>
- <https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage>

校验值（sha256，2026-09-13 下载）：

```text
36a2d7e274d12e1050d0e9ecfe11d339ed54720b2bec464c286d53f8b07f5c62  linuxdeploy-x86_64.AppImage
cfc1055b2b9dbc08412b579f20990b7b41a17b61beaa5847dc9477c96c9e9617  linuxdeploy-plugin-qt-x86_64.AppImage
a6d71e2b6cd66f8e8d16c37ad164658985e0cf5fcaa950c90a482890cb9d13e0  appimagetool-x86_64.AppImage
```

## 为什么入库而不是按需下载

三个文件合计约 46 MiB，入库确实让仓库变大。之所以这么做：国内网络下
GitHub 的 release 资源 CDN（`objects.githubusercontent.com`）常常不可达，
按需下载会让打包在国内环境下直接失败——入库是"开箱可用"与"离线可用"的
唯一稳妥选择。若将来要改回按需下载，记得同步 `scripts/make_appimage.sh`
的检查逻辑与本文件。

## 更新方式

删除旧文件后重新下载对应文件，`chmod +x`，并同步更新本文件的版本与 sha256。
三个都是上游 `continuous` 构建，无正式版本号，内容会随上游变动；需要可复现的
打包结果时，按上面的 sha256 核对。

## 许可

三者均为 MIT 许可，随本仓库分发不改变其许可。

## 运行环境

`scripts/make_appimage.sh` 会导出 `APPIMAGE_EXTRACT_AND_RUN=1`，让这三个
AppImage 无需 FUSE 即可自解包运行——容器、CI 以及未启用 FUSE 的 WSL 都能用。
