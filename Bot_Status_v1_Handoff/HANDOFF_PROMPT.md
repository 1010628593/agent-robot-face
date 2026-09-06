# 复制给本地 Agent 的启动提示

下面整段可直接粘贴。先将本包完整解压到实际工作区，保留目录关系。

```text
请按照当前目录 Bot Status v1 文档包实施，不再重新做产品选型。

先读取 AGENTS.md、README.md、docs/01_PRD.md、docs/02_UI_UX.md、
docs/03_STATE_MODEL.md、docs/05_PROTOCOL.md、docs/10_IMPLEMENTATION_PLAN.md。

目标设备是我已收到的 Waveshare ESP32-S3-Touch-AMOLED-1.75-B 黑色带壳版。
Mac 上已通过 EIM 安装 ESP-IDF v6.1。必须复用该环境，不转 Arduino/PlatformIO，
不擅自降级。第一版只做 Face、Agent Picker、Usage/Quota 三个逻辑页面，
支持 Codex、腾讯 WorkBuddy、Cursor、Nous Research Hermes 的状态与能力分级接入。
不涉及我的 Spring AI Alibaba 企业项目，也不做机器人、语音或自动批准。

你可以开始读文件、检查版本、编写项目代码和运行非破坏性测试。
先列出工作区已有内容，再执行 T00–T02：只读盘点、备份条件、官方示例 v6.1 构建。
首次烧录、原厂固件恢复、修改任一 Agent 的用户配置或安装 launchd 服务前，请单独确认。
不要擦除 Flash、写 eFuse、绕过 Hook 信任或读出我的账号凭证。

按任务顺序增量实现。每任务先测试再实现，完成后提交并更新
reports/implementation-log.md；协议以 contracts/ 与 docs/05_PROTOCOL.md 为准。
三屏 mock 完成后继续真实 Adapter，不能把四张卡片或模拟状态当作四源接入完成。
WorkBuddy Desktop 的真实 Hook 需要能力探测；不能直接套 CodeBuddy CLI。
额度不可得就 N/A，提供范围、来源和更新时间；不伪造剩余额度。

请现在完成环境/仓库盘点并开始首个可执行任务。
第一次回复给出检测结果、实施顺序与需要我确认的硬件操作，不要再泛泛写一份新方案。
```

## 手动阅读的最短路径

先看 `START_HERE_macOS.md`；三屏行为看 `docs/02_UI_UX.md`；完整工作拆分看 `docs/10_IMPLEMENTATION_PLAN.md`。

## 不要这样交付

只把某一段聊天摘要粘给 Agent，容易丢失统计口径、手势冲突处理、B/C 板型限制和恢复流程。应传整个文件夹，至少同时提供 AGENTS、PRD、协议、任务计划和对应 JSON Schema。
