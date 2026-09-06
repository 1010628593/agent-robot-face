# Bot Status — 本地实施 Agent 必须遵循

## 项目与范围

你要实现 ESP32-S3-Touch-AMOLED-1.75-B 的桌面 Bot Status v1。用户已有黑色 B 版，macOS 上已通过 EIM 安装并激活 ESP-IDF v6.1。需求已确认：三屏、状态表情、触摸/滑动、长按 Agent 选择、Codex / WorkBuddy / Cursor / Hermes、usage/quota。

- 不要继续推荐购买硬件。
- 不引入 Spring AI Alibaba、Spring Cloud 或企业 Agent 服务；也不做 robot、音频、摄像头。
- 不迁移到 Arduino / PlatformIO；不擅自降级 ESP-IDF。v6.1 适配问题先定位 BSP/API，再报告最小修复。
- 当前目录已有代码时先读、盘点、运行测试；禁止清空目录后重建、覆盖用户改动或强推分支。

## 必须先读

`README.md`、`docs/01_PRD.md`、`docs/02_UI_UX.md`、`docs/03_STATE_MODEL.md`、`docs/05_PROTOCOL.md`、`docs/10_IMPLEMENTATION_PLAN.md`。

## 硬件安全

1. 先核对芯片、Flash、PCB revision、USB 端口。B 用标准 1.75 工程，不使用 C/1.85 工程。
2. 首次写入前：私有完整原厂备份、长度和 SHA-256、构建日志、待写固件哈希、明确的人工烧录确认。备份不可提交仓库。
3. 本次授权是本地开发，不等于允许任意硬件擦除。禁止自动 `erase-flash`、改 eFuse、启用 Secure Boot/Flash Encryption、改 PMIC rail 或强制解锁读保护。
4. 固件损坏时保持 ROM 下载/恢复路径；不要把日常调试流程写成每次全盘擦除。
5. 不同时启动 monitor / Bridge / esptool 占用同一串口。

## 事实与来源

- 按当前本机版本发现能力，不根据品牌名声推断接口。
- WorkBuddy 是腾讯桌面工作台；CodeBuddy CLI 的 Hook 不能未经实测就标成 WorkBuddy Desktop 支持。
- Hermes 指 Nous Research Hermes Agent；不是用户自建系统。CLI 应选择支持 CLI 的插件/Hook，不照搬 Gateway-only Hook。
- 不读出/显示/上传 API Key、Cookie、完整 Prompt、工具参数、聊天文本和绝对项目路径。用白名单字段、哈希会话 ID、短标签。
- 不调用未证实的私有计费端点，不硬编码套餐额度，不把 token/context/credit/currency 混成一个百分比。
- `exact` 只说明来源测量精度；`coverage` 仍可能为 partial。每个统计字段都保留来源、范围和时间。
- 四张 Agent 卡片 + 模拟动画不等于完成四个真实接入。维护能力报告与发布门禁。

## 行为边界

- 设备切换 Agent 仅改变观察对象。不得停止当前任务、切换账号、创建任务、发送消息或批准工具。
- Hook 必须旁路、快速、失败不改变 Agent 决策；不通过 stdout 注入上下文或 continuation。
- 新增/修改/卸载用户工具的 Hook、插件、launchd 前，输出最小 diff、备份位置和回滚方式并获得确认。
- WorkBuddy MCP 降级只能标记 `reported`，人工状态只能 `manual`。它们不是完整生命周期观测。
- BLE/Wi-Fi、SwiftUI/Tauri、通知中心、语音、自动前台应用追随均非 v1 必需，不抢先实现。

## 开发规约

- 用官方 BSP 初始化 CO5300/CST9217/PMIC，不猜 GPIO。锁定 vendor commit、BSP、LVGL 和 `dependencies.lock`。
- 所有 LVGL 对象更新在唯一 UI 所有者上下文中；接收线程只校验/入队；禁止网络回调里直接操作 LVGL。
- 所有队列、字符串、缓存、重试都有上限；运行态只存 RAM。
- 先写可失败的测试/夹具，再最小实现；每任务一条可回退提交。
- 所有测试向量使用固定时钟/随机种子；不要靠等待现实时间验证超时。
- 保活不重启眨眼/完成动画；乱序旧事件不回滚新 run；同一 usage 不重复入账。
- 累计 token 是快照，不是增量；缺失值不能用 0 代填。
- 暗屏第一次触摸只唤醒，抬手前不得继续触发单击/长按。
- UI 色彩：状态色固定含义，Agent 主题色只用于标识，不能把成功/错误颜色改成品牌色。

## 交付与报告

每任务结束更新 `reports/implementation-log.md`（实施时创建）：变更文件、执行命令、测试结果、硬件是否实际验证、剩余风险、下个任务。

只允许以下实证描述：`文档校验通过`、`主机测试通过`、`ESP-IDF 编译通过`、`已烧录`、`实机验收通过`。不可互相替代。

无设备访问时完成 host/UI simulator 工作并标 `BLOCKED_HARDWARE`。来源不可用时标 `BLOCKED_SOURCE` 并说明具体证据，不伪造数据，不把整个项目停在讨论中。
