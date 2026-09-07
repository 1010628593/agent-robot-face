# Agent Robot Face

ESP32-S3 圆屏 Agent 状态终端，配套独立 Python Mac Bridge 与原生菜单栏应用。

生产默认使用真实来源，未接入或失联不会回退到模拟数据。四个来源分别展示已验证的生命周期能力；接入配置存在不代表所有状态已经验收。

## 当前产品

- 固定淡蓝白眼睛与黑色瞳孔，恒定填充颜色；业务状态、抚摸动作、自然眨眼分层。
- 分区轻点、连续抚摸与双指表情互动；顶部下拉选择 Agent、底部上滑打开用量，从面板对侧边缘反向推回；取消长按导航，保留触摸朝向锁定。
- 中文 Agent 选择页与任务／今日统计／额度信息页，真实缺失和过期提示。
- v2 USB 协议，主机确认选择，自动关注／固定 Agent；Bridge 独占串口并在烧录时释放。
- SQLite 元数据账本、幂等 Hook 合并及回滚备份、用户级 LaunchAgent、原生菜单栏控制。

## Mac 使用

```sh
tools/bridge install
host-app/scripts/install.sh
open "$HOME/Applications/Agent Robot Face.app"
```

点击菜单栏“双眼”图标展开面板。Bridge 与菜单应用独立运行；退出菜单应用不停止 USB 或来源观测。首次启用 Hermes 的本项目 Hook 时，其原生信任提示仍由 Hermes 管理。

```sh
tools/bridge doctor
tools/bridge state
tools/bridge auto
tools/bridge pin cursor
tools/bridge pause
tools/bridge resume
tools/bridge flash -- <烧录命令及参数>
```

不要在 Bridge 连接期间另开串口监视器。卸载本项目 Hook 使用 `tools/bridge uninstall`，菜单应用单独使用 `host-app/scripts/uninstall.sh`。原有 Agent Keyboard、Memmy 等配置不由本项目删除。

## 验收与能力边界

2.2.0 常驻 Face、边缘悬浮卡片与跟手弹簧规范见 [OS 动效设计](docs/os-motion-design.md) 和 [本次验收](reports/os-motion/acceptance.md)。

2.1.0 触摸改造记录见 [触摸交互契约](docs/touch-interactions.md) 与 [触摸验收记录](reports/touch-interactions/acceptance.md)。下述 2.0.0 记录是此前生产链路的验收证据，不代表新双指交互已通过实机验收。

当前固件 2.0.0 已实际烧录；四源已有真实任务记录投影到设备并核对设备回读。用户确认当前显示与交互正常。**尚不能宣称四源全部生命周期验收完成**：WorkBuddy Desktop 的真实 Stop Hook 缺少终态分类，取消、失败、等待仍未验证；其他来源也按每项实际证据展示。官方额度不可用时显示暂无来源。

- [本批交付与实机验收](reports/formal-product/acceptance-2026-09-07.md)
- [逐来源能力](reports/formal-product/source-capabilities.md)
- [Bridge 安装与服务](docs/bridge-service.md)
- [Mac 菜单栏](docs/mac-app.md)
- [USB v2 契约](docs/protocol-v2.md)
- [本机控制 API](docs/host-api.md)
- [已批准实施计划](docs/superpowers/plans/2026-09-07-formal-product.md)

模拟器仅用于显式开发模式 `CONFIG_BOT_DEV_SIM`；生产构建默认关闭。历史 v1 交接资料位于 `Bot_Status_v1_Handoff/`，不代表当前生产协议。
