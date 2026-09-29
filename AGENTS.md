# AGENTS.md

本仓库的规则、约定与常用命令统一维护在 [CLAUDE.md](CLAUDE.md)，动手前先读它。

文档在 `docs/`：

| 文档 | 内容 |
|------|------|
| [模板使用指南](docs/guide/模板使用指南.md) | 必改项、目录结构、单元测试、扩展指引、Linux 构建 |
| [发布指南](docs/guide/发布指南.md) | 打 tag、逐架构打包、产物说明、常见问题 |
| [构建配置与崩溃排查](docs/guide/构建配置与崩溃排查.md) | 各构建配置的差异、符号归档与崩溃转储解析 |
| [funq 注入式 UI 测试](docs/guide/funq注入式UI测试.md) | 零侵入的界面回归：注入被测进程、用 Python 写用例、控件路径怎么查 |
| [调试 libFunq](docs/guide/调试libFunq.md) | 注入库没有可 F5 的工程：附加到进程、断在注入前后的步骤与坑 |
| [第三方依赖补丁](docs/guide/第三方依赖补丁.md) | 上游源码怎么改才留得住：补丁目录约定、option 与补丁的分工、接新依赖的检查单 |

历史方案与未了结的坑在 [docs/plan/plan总结.md](docs/plan/plan总结.md)。
